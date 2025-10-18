#include <string.h>

#include "wifi_config_ap.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "wifi_config_ap";

#ifndef CONFIG_WIFI_CONFIG_AP_MAX_RETRY
#define CONFIG_WIFI_CONFIG_AP_MAX_RETRY 5
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* Event bits */
#define WIFI_AP_STARTED_BIT BIT0
#define WIFI_AP_STOPPED_BIT BIT1

/* State and netif */
static wifi_config_ap_state_t s_state = WIFI_CONFIG_AP_STATE_IDLE;
static esp_netif_t *s_netif = NULL;

/* Default config */
#ifndef CONFIG_WIFI_CONFIG_AP_SSID
#define CONFIG_WIFI_CONFIG_AP_SSID "ESP32-Setup"
#endif
#ifndef CONFIG_WIFI_CONFIG_AP_PASSWORD
#define CONFIG_WIFI_CONFIG_AP_PASSWORD ""
#endif
#ifndef CONFIG_WIFI_CONFIG_AP_CHANNEL
#define CONFIG_WIFI_CONFIG_AP_CHANNEL 1
#endif
#ifndef CONFIG_WIFI_CONFIG_AP_MAX_CONNECTIONS
#define CONFIG_WIFI_CONFIG_AP_MAX_CONNECTIONS 4
#endif

/* Static AP Settings */
static const char *s_ap_ssid = NULL;
static const char *s_ap_password = NULL;
static int s_ap_channel = CONFIG_WIFI_CONFIG_AP_CHANNEL;
static int s_ap_max_connections = CONFIG_WIFI_CONFIG_AP_MAX_CONNECTIONS;

/* Wi-Fi Configuration struct */
static wifi_config_t s_wifi_ap_cfg = {0};

static void wifi_ap_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
    {
        s_state = WIFI_CONFIG_AP_STATE_STARTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_AP_STARTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi AP started");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP)
    {
        s_state = WIFI_CONFIG_AP_STATE_STOPPED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_AP_STOPPED_BIT);
        ESP_LOGI(TAG, "Wi-Fi AP stopped");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " connected, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " disconnected, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

/*========== NVS Storage Initialize ==========*/
static esp_err_t ensure_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t dhcp_server_configure(esp_netif_t *netif)
{
    if (!netif)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_netif_dhcps_stop(netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    {
        ESP_LOGE(TAG, "Failed to stop DHCP server: %s", esp_err_to_name(err));
        return err;
    }

    // Configure static IP for the AP
    esp_netif_ip_info_t ip_info = {};
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    err = esp_netif_set_ip_info(netif, &ip_info);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set IP info: %s", esp_err_to_name(err));
        return err;
    }

    /**  Set DNS server to the AP IP **/
#if ESP_IDF_VERSION_MAJOR >= 4
    esp_netif_dns_info_t dns = {0};
    dns.ip.u_addr.ip4 = ip_info.gw; // main DNS = 192.168.4.1
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
#endif
    ESP_LOGI(TAG, "DHCP server configured: IP=" IPSTR " GW=" IPSTR " MASK=" IPSTR,
             IP2STR(&ip_info.ip), IP2STR(&ip_info.gw), IP2STR(&ip_info.netmask));
    return ESP_OK;
}

static esp_err_t dhcp_server_start(esp_netif_t *netif)
{
    if (!netif)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_netif_dhcps_start(netif);
    if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
    {
        ESP_LOGW(TAG, "DHCP server already started");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start DHCP server: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "DHCP server started on netif");
    return ESP_OK;
}

static esp_err_t dhcp_server_stop(esp_netif_t *netif)
{
    if (!netif)
        return ESP_ERR_INVALID_ARG;
    esp_err_t err = esp_netif_dhcps_stop(netif);
    if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    {
        ESP_LOGW(TAG, "DHCP server already stopped");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop DHCP server: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "DHCP server stopped on netif");
    return ESP_OK;
}

/*========== Public Functions ==========*/
esp_err_t wifi_config_ap_init(wifi_config_ap_settings_t *settings)
{
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group)
        return ESP_ERR_NO_MEM;

    /* Initilize Netif and NVS */
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK)
        return err;

    err = ensure_nvs_init();
    if (err != ESP_OK)
        return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    if (!s_netif)
    {
        s_netif = esp_netif_create_default_wifi_ap();
    }

    /* Initialize Wi-Fi */
    wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wicfg);
    if (err != ESP_OK)
        return err;

    /* Event instances register */
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_ap_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    // Set Wi-Fi mode to AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    /* Select SSID, Password, Max Retry from cfg or Kconfig */
    const char *ssid = (settings && settings->ssid) ? settings->ssid : CONFIG_WIFI_CONFIG_AP_SSID;
    const char *pass = (settings && settings->password) ? settings->password : CONFIG_WIFI_CONFIG_AP_PASSWORD;
    int channel = (settings && settings->channel > 0) ? settings->channel : CONFIG_WIFI_CONFIG_AP_CHANNEL;
    int max_connections = (settings && settings->max_connections > 0) ? settings->max_connections : CONFIG_WIFI_CONFIG_AP_MAX_CONNECTIONS;

    /* Update static variable */
    s_ap_ssid = ssid;
    s_ap_password = pass;
    s_ap_channel = channel;
    s_ap_max_connections = max_connections;

    /* Configure the AP settings */
    memset(&s_wifi_ap_cfg, 0, sizeof(s_wifi_ap_cfg));
    strncpy((char *)s_wifi_ap_cfg.ap.ssid, ssid, sizeof(s_wifi_ap_cfg.ap.ssid) - 1);
    strncpy((char *)s_wifi_ap_cfg.ap.password, pass, sizeof(s_wifi_ap_cfg.ap.password) - 1);
    s_wifi_ap_cfg.ap.ssid_len = strlen(ssid);
    s_wifi_ap_cfg.ap.channel = channel;
    s_wifi_ap_cfg.ap.max_connection = max_connections;
    s_wifi_ap_cfg.ap.ssid_hidden = 0;

    // Auth & PMF
    if (pass[0] == '\0')
    {
        s_wifi_ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
        s_wifi_ap_cfg.ap.pmf_cfg.capable = false;
        s_wifi_ap_cfg.ap.pmf_cfg.required = false;
    }
    else
    {
        s_wifi_ap_cfg.ap.authmode = WIFI_AUTH_WPA2_WPA3_PSK;
        s_wifi_ap_cfg.ap.pmf_cfg.capable = true;
        s_wifi_ap_cfg.ap.pmf_cfg.required = false;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &s_wifi_ap_cfg));

    /* Update state */
    s_state = WIFI_CONFIG_AP_STATE_IDLE;
    ESP_LOGI(TAG, "Wifi_init_softap finished. SSID:%s password:%s channel:%d",
             s_ap_ssid, s_ap_password, s_ap_channel);

    /* Configure and start DHCP server */
    err = dhcp_server_configure(s_netif);
    if (err != ESP_OK)
    {
        return err;
    }
    err = dhcp_server_start(s_netif);
    if (err != ESP_OK)
    {
        return err;
    }
    /* Auto start if configured */
    if (settings && settings->auto_start)
    {
        return wifi_config_ap_start();
    }

    return ESP_OK;
}

esp_err_t wifi_config_ap_start(void)
{
    // Ensure that Wi-Fi EvenGroup had been initialized
    if (!s_wifi_event_group)
        return ESP_ERR_INVALID_STATE;
    esp_err_t start_err = esp_wifi_start();
    if (start_err != ESP_OK)
        return start_err;
    // WIFI_EVENT_AP_START will be handled in event handler
    return ESP_OK;
}

esp_err_t wifi_config_ap_stop(void)
{
    esp_err_t stop_err = esp_wifi_stop();
    if (stop_err != ESP_OK)
        return stop_err;
    // WIFI_EVENT_AP_STOP will be handled in event handler
    return ESP_OK;
}

wifi_config_ap_state_t wifi_config_ap_get_state(void)
{
    return s_state;
}

esp_netif_t *wifi_config_ap_get_netif(void)
{
    return s_netif;
}