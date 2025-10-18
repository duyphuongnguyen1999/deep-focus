#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/** Namespace and keys in NVS **/
#ifndef WIFI_NVS_NAMESPACE
#define WIFI_NVS_NAMESPACE "wifi_config"
#endif
#ifndef WIFI_NVS_KEY_SSID
#define WIFI_NVS_KEY_SSID "ssid"
#endif
#ifndef WIFI_NVS_KEY_PASSWORD
#define WIFI_NVS_KEY_PASSWORD "password"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Get Wi-Fi SSID and password from NVS
     *
     * @param ssid Buffer to store SSID
     * @param ssid_size Size of SSID buffer
     * @param password Buffer to store password
     * @param password_size Size of password buffer
     *
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t wifi_nvs_get(char *ssid, size_t ssid_size, char *password, size_t password_size);

    /**
     * @brief Set Wi-Fi SSID and password to NVS
     *
     * @param ssid SSID string
     * @param password Password string
     *
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t wifi_nvs_set(const char *ssid, const char *password);

    /**
     * @brief Clear Wi-Fi SSID and password from NVS
     */
    esp_err_t wifi_nvs_clear(void);

    /**
     * @brief Check if Wi-Fi credentials exist in NVS
     */
    bool wifi_nvs_exists(void);

#ifdef __cplusplus
}
#endif