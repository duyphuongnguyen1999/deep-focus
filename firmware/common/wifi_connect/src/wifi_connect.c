#include "wifi_connect.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "wifi_conn";

#ifndef CONFIG_WIFI_CONN_MAX_RETRY
#define CONFIG_WIFI_CONN_MAX_RETRY 5
#endif

/* Event bits */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static wifi_conn_state_t s_state = WIFI_CONN_STATE_IDLE;
static esp_netif_t *s_netif = NULL;
static wifi_config_t s_wifi_cfg = {0};
static int s_max_retry_cfg = CONFIG_WIFI_CONN_MAX_RETRY;

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
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
    }
}

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

esp_err_t wifi_conn_init(const wifi_conn_config_t *cfg)
{
    ESP_ERROR_CHECK(ensure_nvs_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (!s_netif)
    {
        s_netif = esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wicfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    s_wifi_event_group = xEventGroupCreate();

    /* chọn SSID/PASS */
    const char *ssid = (cfg && cfg->ssid) ? cfg->ssid : CONFIG_WIFI_CONN_SSID;
    const char *pass = (cfg && cfg->password) ? cfg->password : CONFIG_WIFI_CONN_PASSWORD;
    s_max_retry_cfg = (cfg && cfg->max_retry >= 0) ? cfg->max_retry : CONFIG_WIFI_CONN_MAX_RETRY;

    memset(&s_wifi_cfg, 0, sizeof(s_wifi_cfg));
    strncpy((char *)s_wifi_cfg.sta.ssid, ssid, sizeof(s_wifi_cfg.sta.ssid) - 1);
    strncpy((char *)s_wifi_cfg.sta.password, pass, sizeof(s_wifi_cfg.sta.password) - 1);

    s_wifi_cfg.sta.threshold.authmode = (strlen(pass) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    s_wifi_cfg.sta.pmf_cfg.capable = true;
    s_wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &s_wifi_cfg));

    s_state = WIFI_CONN_STATE_IDLE;
    s_retry_num = 0;

    if (cfg && cfg->auto_start)
    {
        return wifi_conn_start();
    }
    return ESP_OK;
}

esp_err_t wifi_conn_start(void)
{
    if (!s_wifi_event_group)
        return ESP_ERR_INVALID_STATE;
    ESP_ERROR_CHECK(esp_wifi_start());
    // WIFI_EVENT_STA_START sẽ gọi esp_wifi_connect() trong handler
    return ESP_OK;
}

esp_err_t wifi_conn_stop(void)
{
    if (!s_wifi_event_group)
        return ESP_ERR_INVALID_STATE;
    esp_wifi_disconnect();
    esp_wifi_stop();
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
