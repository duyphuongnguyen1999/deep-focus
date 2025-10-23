#pragma once
#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Type definition for the Wi-Fi credentials save handler.
     *
     * This callback function is invoked when the user submits the Wi-Fi
     * credentials (SSID and password) through the provisioning HTTP server.
     *
     * @param ssid The SSID of the Wi-Fi network.
     * @param pass The password of the Wi-Fi network.
     * @param user_ctx User-defined context pointer passed during registration.
     */
    typedef void (*wifi_prov_http_handle_save_t)(const char *ssid, const char *pass, void *user_ctx);

    /**
     * @brief Register a handler for saving Wi-Fi credentials.
     *
     * This function allows the application to register a callback that will be
     * called when the user submits Wi-Fi credentials through the provisioning
     * HTTP server.
     *
     * @param save_handler The callback function to handle saving Wi-Fi credentials.
     * @param user_ctx A user-defined context pointer that will be passed to the
     *                 callback when invoked.
     */
    void wifi_prov_http_register_save_handler(wifi_prov_http_handle_save_t save_handler, void *user_ctx);

    /**
     * @brief Initialize the Wi-Fi provisioning HTTP server.
     *
     * This function sets up and starts the HTTP server that serves the Wi-Fi
     * provisioning web pages. It must be called before starting the SoftAP.
     *
     * @return
     *      - ESP_OK on success
     *      - ESP_ERR_NO_MEM if memory allocation fails
     *      - ESP_ERR_INVALID_STATE if the server is already running
     */
    esp_err_t wifi_prov_http_init(void);

    /**
     * @brief Deinitialize the Wi-Fi provisioning HTTP server.
     *
     * This function stops and cleans up the HTTP server used for Wi-Fi
     * provisioning. It should be called when the provisioning process is complete
     * or no longer needed.
     *
     * @return
     *      - ESP_OK on success
     *      - ESP_ERR_INVALID_STATE if the server is not running
     */
    esp_err_t wifi_prov_http_deinit(void);

#ifdef __cplusplus
}
#endif