#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_check.h"
#include "lwip/inet.h"

#include "app_config.h"
#include "app_runtime.h"

#include "wifi_connect.h"   // STA module
#include "wifi_config_ap.h" // AP + DHCP module
#include "wifi_prov_http.h" // HTTP form /save
#include "wifi_nvs.h"       // NVS creds

#include "dht11_reader.h"
#include "i2c_oled_display.h"
#include "global.h"

static const char *TAG = "APP_RUNTIME";

/*========== Pipeline event group ==========*/
static EventGroupHandle_t s_runtime_eg = NULL;
#define EV_SAVED_BIT BIT0 // nhận khi HTTP /save đã lưu NVS

/*========== HTTP saved callback ==========*/
static void on_prov_saved(const char *ssid, const char *pass, void *ctx)
{
    (void)ssid;
    (void)pass;
    (void)ctx;
    if (s_runtime_eg)
        xEventGroupSetBits(s_runtime_eg, EV_SAVED_BIT);
}

/*========== Helper: Try to connect STA by creds in NVS ==========*/
static bool try_sta_from_nvs(int wait_ip_timeout_ms)
{
    char ssid[33], pass[65];
    if (wifi_nvs_get_creds(ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK)
    {
        ESP_LOGW(TAG, "No Wi-Fi creds in NVS");
        return false;
    }

    wifi_conn_config_t cfg = {
        .ssid = ssid,
        .password = pass,
        .max_retry = CONFIG_WIFI_CONN_MAX_RETRY, // or -1
        .auto_start = true,
    };
    esp_err_t e = wifi_conn_init(&cfg);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "wifi_conn_init failed: %s", esp_err_to_name(e));
        return false;
    }

    e = wifi_conn_wait_ip(wait_ip_timeout_ms);
    if (e == ESP_OK)
    {
        uint32_t ip;
        if (wifi_conn_get_ipv4(&ip))
        {
            struct in_addr a = {.s_addr = ip};
            ESP_LOGI(TAG, "STA ready (NVS), IP: %s", inet_ntoa(a));
        }
        else
        {
            ESP_LOGI(TAG, "STA ready (NVS), IP obtained");
        }
        return true;
    }

    ESP_LOGW(TAG, "STA connect failed/timeout (NVS)");
    (void)wifi_conn_stop();
    return false;
}

/*========== Helper: Start AP + HTTP provisioning ==========*/
static esp_err_t start_ap_and_http(const char *ap_ssid, const char *ap_pass, int ch, int max_conn)
{
    wifi_config_ap_settings_t ap = {
        .ssid = ap_ssid,
        .password = ap_pass,
        .channel = ch,
        .max_connections = max_conn,
        .auto_start = true,
    };

    ESP_RETURN_ON_ERROR(wifi_config_ap_init(&ap), TAG, "AP init failed");
    wifi_prov_http_register_save_handler(on_prov_saved, NULL);
    ESP_RETURN_ON_ERROR(wifi_prov_http_init(), TAG, "HTTP start failed");

    ESP_LOGI(TAG, "AP up SSID:%s (pass:%s), browse http://192.168.4.1/", ap_ssid, ap_pass);
    return ESP_OK;
}

/*========== Helper: Stop AP + HTTP ==========*/
static void stop_ap_and_http(void)
{
    wifi_prov_http_deinit();
    (void)wifi_config_ap_stop();
}

/*========== LVGL ========== */
// Handle display for "update_task"
static lv_display_t *s_disp = NULL;

// Update OLED screen every second
static void updater_task(void *arg)
{
    (void)arg;
    while (1)
    {
        int16_t t10 = global_temperature;
        int16_t h10 = global_humidity;

        bool valid = (t10 != INT16_MIN) && (h10 != INT16_MIN);
        float temp_c = valid ? (float)t10 / 10.0f : NAN;
        float hum_pct = valid ? (float)h10 / 10.0f : NAN;

        oled_display_update(temp_c, hum_pct); // if NAN -> display “--.--” or similar

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*========== Wi-fi Orchestration ========== */
static esp_err_t start_wifi(void)
{
    // 1. Create event group
    if (!s_runtime_eg)
    {
        s_runtime_eg = xEventGroupCreate();
        if (!s_runtime_eg)
        {
            ESP_LOGE(TAG, "Create event group failed");
            return ESP_ERR_NO_MEM;
        }
    }

    // 2. Try connect STA from NVS
    if (try_sta_from_nvs(/*wait_ip_timeout_ms=*/15000))
    {
        return ESP_OK; // connected ok
    }

    // 3. Connect failed, start AP + HTTP provisioning -> try config from menuconfig
    {
        wifi_conn_config_t wifi_cfg = {
            .ssid = NULL, // Use menuconfig configuration
            .password = NULL,
            .max_retry = -1, // Use CONFIG_WIFI_CONN_MAX_RETRY
            .auto_start = true,
        };

        if (wifi_conn_init(&wifi_cfg) == ESP_OK && wifi_conn_wait_ip(15000) == ESP_OK)
        {
            uint32_t ip;
            if (wifi_conn_get_ipv4(&ip))
            {
                struct in_addr a = {.s_addr = ip};
                ESP_LOGI(TAG, "Wi-Fi ready (menuconfig), IP: %s", inet_ntoa(a));
            }
            return ESP_OK; // connected ok
        }
        (void)wifi_conn_stop();
        ESP_LOGW(TAG, "STA (Kconfig) failed; fallback to provisioning AP.");
    }

    // 4. Start AP + HTTP provisioning
    ESP_RETURN_ON_ERROR(
        start_ap_and_http(
            CONFIG_WIFI_CONFIG_AP_SSID,
            CONFIG_WIFI_CONFIG_AP_PASSWORD,
            CONFIG_WIFI_CONFIG_AP_CHANNEL,
            CONFIG_WIFI_CONFIG_AP_MAX_CONNECTIONS),
        TAG,
        "start_ap_and_http failed");

    // 5. Wait for HTTP /save event
    EventBits_t bits = xEventGroupWaitBits(
        s_runtime_eg,
        EV_SAVED_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);

    // 6. Stop AP + HTTP provisioning and try connect STA again
    stop_ap_and_http();

    if (!(bits & EV_SAVED_BIT))
    {
        ESP_LOGW(TAG, "Provisioning timeout. Offline mode.");
        return ESP_ERR_TIMEOUT;
    }

    // 7. Try connect STA from NVS again
    if (try_sta_from_nvs(15000))
    {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Provisioning done but STA connect failed.");
    return ESP_FAIL;
}

/*========== Read DHT11 Data ==========*/
static esp_err_t start_dht11_task(void)
{
    static dht11_t dht11_sensor;
    dht11_sensor = dht11_init(DHT11_PIN);

    BaseType_t ok = xTaskCreate(
        read_dht11_and_update_globals_task,
        "dht11_task",
        3072,
        &dht11_sensor,
        5,
        NULL);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "Create dht11_task failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DHT11 task created");
    return ESP_OK;
}

/*========== OLED Initialization ==========*/
static esp_err_t init_oled_and_ui(void)
{
    s_disp = oled_display_init(I2C_PORT, SDA_IO, SCL_IO, I2C_ADDR, OLED_W, OLED_H);
    if (!s_disp)
    {
        ESP_LOGE(TAG, "Failed to initialize OLED display");
        return ESP_FAIL;
    }
    oled_display_create_basic_ui(s_disp);
    ESP_LOGI(TAG, "OLED display initialized");
    return ESP_OK;
}

// Public APIs
esp_err_t app_runtime_init(void)
{
    // 1. Wi-Fi (still ok in offline mode)
    ESP_ERROR_CHECK_WITHOUT_ABORT(start_wifi());

    // 2. DHT11
    ESP_RETURN_ON_ERROR(start_dht11_task(), TAG, "start_dht11_task failed");

    // 3. OLED + UI
    ESP_RETURN_ON_ERROR(init_oled_and_ui(), TAG, "init_oled_and_ui failed");

    return ESP_OK;
}

esp_err_t app_runtime_start(void)
{
    // Create task update UI
    BaseType_t ok = xTaskCreate(
        updater_task,
        "ui_update",
        4096,
        NULL,
        3,
        NULL);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "Create ui_update task failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "UI update task created");
    return ESP_OK;
}
