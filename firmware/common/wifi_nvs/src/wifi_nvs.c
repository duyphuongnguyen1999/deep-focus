#include <string.h>
#include "wifi_nvs.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "wifi_nvs";

/** Ensure NVS Initialized */
static esp_err_t ensure_nvs_initialized(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS partition was truncated or a new version was found. Erasing...");
        err = nvs_flash_erase();
        ESP_RETURN_ON_ERROR(err, TAG, "Failed to erase NVS flash");

        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to initialize NVS flash");
    return ESP_OK;
}

/** Validate SSID and Password */
static inline bool credential_valid(const char *ssid, const char *pass)
{
    if (!ssid || !pass)
        return false;
    size_t sl = strlen(ssid), pl = strlen(pass);
    if (sl == 0 || sl > 32)
        return false;
    if (pl > 64)
        return false;
    return true;
}

/** Get Wi-Fi SSID and password from NVS */
esp_err_t wifi_nvs_get_creds(char *ssid, size_t ssid_size, char *password, size_t password_size)
{
    esp_err_t err;
    nvs_handle_t nvs_handle;

    err = ensure_nvs_initialized();
    ESP_RETURN_ON_ERROR(err, TAG, "NVS not initialized");

    if (!ssid || ssid_size == 0 || !password || password_size == 0)
        return ESP_ERR_INVALID_ARG;

    err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to open NVS namespace");

    size_t required_ssid_size = ssid_size;
    err = nvs_get_str(nvs_handle, WIFI_NVS_KEY_SSID, ssid, &required_ssid_size);
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGW(TAG, "SSID not found in NVS");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to get SSID from NVS: %s", esp_err_to_name(err));
        }
        return err;
    }

    size_t required_password_size = password_size;
    err = nvs_get_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, password, &required_password_size);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        password[0] = '\0';
        err = ESP_OK;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Get PASS failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);

    // Validate credentials
    if (!credential_valid(ssid, password))
    {
        ESP_LOGW(TAG, "Invalid Wi-Fi credentials retrieved from NVS");
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

esp_err_t wifi_nvs_set_creds(const char *ssid, const char *password)
{
    esp_err_t err;
    nvs_handle_t nvs_handle;

    err = ensure_nvs_initialized();
    ESP_RETURN_ON_ERROR(err, TAG, "NVS not initialized");

    if (!credential_valid(ssid, password))
    {
        ESP_LOGE(TAG, "Invalid SSID or password");
        return ESP_ERR_INVALID_ARG;
    }

    err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to open NVS namespace");

    err = nvs_set_str(nvs_handle, WIFI_NVS_KEY_SSID, ssid);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to set SSID in NVS");

    err = nvs_set_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, password);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to set password in NVS");

    err = nvs_commit(nvs_handle);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to commit changes to NVS");

    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t wifi_nvs_clear(void)
{
    esp_err_t err;
    nvs_handle_t nvs_handle;

    err = ensure_nvs_initialized();
    ESP_RETURN_ON_ERROR(err, TAG, "NVS not initialized");

    err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to open NVS namespace");

    err = nvs_erase_key(nvs_handle, WIFI_NVS_KEY_SSID);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "Failed to erase SSID from NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_key(nvs_handle, WIFI_NVS_KEY_PASSWORD);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(nvs_handle);
        ESP_LOGE(TAG, "Failed to erase password from NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_commit(nvs_handle);

    nvs_close(nvs_handle);

    return err;
}

bool wifi_nvs_exists(void)
{
    if (ensure_nvs_initialized() != ESP_OK)
        return false;

    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK)
        return false;

    size_t ssz = 0;
    esp_err_t e = nvs_get_str(h, WIFI_NVS_KEY_SSID, NULL, &ssz);
    nvs_close(h);

    // Only return true if SSID exists and length > 1 (Password can be empty)
    return (e == ESP_OK && ssz > 1); // SSID length includes null terminator
}
