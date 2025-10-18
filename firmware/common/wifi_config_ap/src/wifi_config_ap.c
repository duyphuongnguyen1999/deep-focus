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

/* Static variables */
static const char *s_ap_ssid = NULL;
static const char *s_ap_password = NULL;
#ifndef CONFIG_WIFI_CONFIG_AP_CHANNEL
#define CONFIG_WIFI_CONFIG_AP_CHANNEL 1
#endif
static int s_ap_channel = CONFIG_WIFI_CONFIG_AP_CHANNEL;
#ifndef CONFIG_WIFI_CONFIG_AP_MAX_CONNECTIONS
#define CONFIG_WIFI_CONFIG_AP_MAX_CONNECTIONS 4
#endif
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

esp_err_t wifi_config_ap_init(wifi_config_ap_settings_t *settings)
{
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group)
        return ESP_ERR_NO_MEM;

    /* Initilize Netif and NVS */
    esp_err_t netif_err = esp_netif_init();
    if (netif_err != ESP_OK)
        return netif_err;

    esp_err_t nvs_err = ensure_nvs_init();
    if (nvs_err != ESP_OK)
        return nvs_err;

    esp_err_t evt_loop_err = esp_event_loop_create_default();
    if (evt_loop_err != ESP_OK && evt_loop_err != ESP_ERR_INVALID_STATE)
        return evt_loop_err;

    if (!s_netif)
    {
        s_netif = esp_netif_create_default_wifi_ap();
    }

    /* Initialize Wi-Fi */
    wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t wifi_init_err = esp_wifi_init(&wicfg);
    if (wifi_init_err != ESP_OK)
        return wifi_init_err;

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