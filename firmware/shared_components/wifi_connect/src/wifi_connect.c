#include <string.h>
#include "wifi_connect.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "wifi_conn";

/*========== FreeRTOS event group to signal when we are connected ==========*/
static EventGroupHandle_t s_wifi_event_group;

/*========== Event bits ==========*/
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

/*========== Static variables ==========*/
static int s_retry_num = 0;
static int s_max_retry_cfg = CONFIG_WIFI_CONN_MAX_RETRY;
static wifi_conn_state_t s_state = WIFI_CONN_STATE_IDLE;
static esp_netif_t *s_netif = NULL;

/*========== Wi-Fi Configuration struct ==========*/
static wifi_config_t s_wifi_cfg = {0};

/*========== Wi-Fi Event Handler ==========*/
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        s_state = WIFI_CONN_STATE_CONNECTING;
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        s_state = WIFI_CONN_STATE_DISCONNECTED;
        if (s_retry_num < s_max_retry_cfg)
        {
            s_retry_num++;
            ESP_LOGW(TAG, "Disconnected, retrying... (%d/%d)", s_retry_num, s_max_retry_cfg);
            esp_wifi_connect();
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_state = WIFI_CONN_STATE_FAILED;
            ESP_LOGE(TAG, "Failed to connect after %d retries", s_retry_num);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        s_retry_num = 0;
        s_state = WIFI_CONN_STATE_GOT_IP;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Got IPv4: " IPSTR, IP2STR(&e->ip_info.ip));
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

/**========== Public Functions ==========*/
esp_err_t wifi_conn_init(const wifi_conn_config_t *cfg)
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
        s_netif = esp_netif_create_default_wifi_sta();
    }

    /* Initialize Wi-Fi */
    wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t wifi_init_err = esp_wifi_init(&wicfg);
    if (wifi_init_err != ESP_OK)
        return wifi_init_err;

    /* Event instances register */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Set Wi-Fi mode to STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /* Select SSID, Password, Max Retry from cfg or Kconfig */
    const char *ssid = (cfg && cfg->ssid) ? cfg->ssid : CONFIG_WIFI_CONN_SSID;
    const char *pass = (cfg && cfg->password) ? cfg->password : CONFIG_WIFI_CONN_PASSWORD;

    memset(&s_wifi_cfg, 0, sizeof(s_wifi_cfg));
    strncpy((char *)s_wifi_cfg.sta.ssid, ssid, sizeof(s_wifi_cfg.sta.ssid) - 1);
    strncpy((char *)s_wifi_cfg.sta.password, pass, sizeof(s_wifi_cfg.sta.password) - 1);

    // Auth & PMF
    if (pass[0] == '\0')
    {
        s_wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
        s_wifi_cfg.sta.pmf_cfg.capable = false;
        s_wifi_cfg.sta.pmf_cfg.required = false;
    }
    else
    {
        s_wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        s_wifi_cfg.sta.pmf_cfg.capable = true;
        s_wifi_cfg.sta.pmf_cfg.required = false;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &s_wifi_cfg));

    /* Initial static config variables */
    s_max_retry_cfg = (cfg && cfg->max_retry >= 0) ? cfg->max_retry : CONFIG_WIFI_CONN_MAX_RETRY;
    s_state = WIFI_CONN_STATE_IDLE;

    if (cfg && cfg->auto_start)
    {
        return wifi_conn_start();
    }

    return ESP_OK;
}

esp_err_t wifi_conn_set_wifi_config(const wifi_config_t *wifi_cfg)
{
    if (!wifi_cfg)
        return ESP_ERR_INVALID_ARG;

    memcpy(&s_wifi_cfg, wifi_cfg, sizeof(s_wifi_cfg));

    esp_err_t set_mode_err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (set_mode_err != ESP_OK)
        return set_mode_err;

    esp_err_t set_cfg_err = esp_wifi_set_config(WIFI_IF_STA, &s_wifi_cfg);
    if (set_cfg_err != ESP_OK)
        return set_cfg_err;

    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && (mode & WIFI_MODE_STA))
    {
        // If Wi-Fi is already in STA mode, reconnect with new config
        esp_wifi_disconnect();
        s_retry_num = 0;
        esp_wifi_connect();
    }

    s_state = WIFI_CONN_STATE_IDLE;
    return ESP_OK;
}

esp_err_t wifi_conn_set_ssid_password(const char *ssid, const char *password)
{
    // Validate input
    if (!ssid || !password)
        return ESP_ERR_INVALID_ARG;
    if (strlen(ssid) == 0 || strlen(ssid) >= sizeof(s_wifi_cfg.sta.ssid))
        return ESP_ERR_INVALID_ARG;
    if (strlen(password) >= sizeof(s_wifi_cfg.sta.password))
        return ESP_ERR_INVALID_ARG;

    // Update internal wifi_config_t
    memset(s_wifi_cfg.sta.ssid, 0, sizeof(s_wifi_cfg.sta.ssid));
    memset(s_wifi_cfg.sta.password, 0, sizeof(s_wifi_cfg.sta.password));

    strncpy((char *)s_wifi_cfg.sta.ssid, ssid, sizeof(s_wifi_cfg.sta.ssid) - 1);
    strncpy((char *)s_wifi_cfg.sta.password, password, sizeof(s_wifi_cfg.sta.password) - 1);

    // Auth & PMF
    if (password[0] == '\0')
    {
        s_wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
        s_wifi_cfg.sta.pmf_cfg.capable = false;
        s_wifi_cfg.sta.pmf_cfg.required = false;
    }
    else
    {
        s_wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        s_wifi_cfg.sta.pmf_cfg.capable = true;
        s_wifi_cfg.sta.pmf_cfg.required = false;
    }

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &s_wifi_cfg);

    if (err != ESP_OK)
        return err;

    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && (mode & WIFI_MODE_STA))
    {
        // If Wi-Fi is already in STA mode, reconnect with new config
        esp_wifi_disconnect();
        s_retry_num = 0;
        esp_wifi_connect();
    }

    s_state = WIFI_CONN_STATE_IDLE;

    return ESP_OK;
}

const wifi_config_t *wifi_conn_get_wifi_config(void)
{
    return &s_wifi_cfg;
}

esp_err_t wifi_conn_start(void)
{
    // Reset retry counter
    s_retry_num = 0;
    // Ensure that Wi-Fi EvenGroup had been initialized
    if (!s_wifi_event_group)
        return ESP_ERR_INVALID_STATE;
    esp_err_t start_err = esp_wifi_start();
    if (start_err != ESP_OK)
        return start_err;
    // WIFI_EVENT_STA_START will call esp_wifi_connect() in handler
    return ESP_OK;
}

esp_err_t wifi_conn_stop(void)
{
    if (!s_wifi_event_group)
        return ESP_ERR_INVALID_STATE;
    esp_wifi_disconnect();
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED)
        return err;
    s_state = WIFI_CONN_STATE_IDLE;
    s_retry_num = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    return ESP_OK;
}

wifi_conn_state_t wifi_conn_get_state(void)
{
    return s_state;
}

esp_err_t wifi_conn_wait_ip(int timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        (timeout_ms <= 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT)
        return ESP_OK;
    if (bits & WIFI_FAIL_BIT)
        return ESP_FAIL;
    return ESP_ERR_TIMEOUT;
}

bool wifi_conn_get_ipv4(uint32_t *ip_u32)
{
    if (!s_netif || !ip_u32)
        return false;
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(s_netif, &ip) == ESP_OK)
    {
        *ip_u32 = ip.ip.addr; // network byte order
        return ip.ip.addr != 0;
    }
    return false;
}
