#pragma once
#include "esp_wifi.h"
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif
    typedef enum
    {
        WIFI_CONFIG_AP_STATE_IDLE = 0,
        WIFI_CONFIG_AP_STATE_STARTED,
        WIFI_CONFIG_AP_STATE_STOPPED,
        WIFI_CONFIG_AP_STATE_FAILED
    } wifi_config_ap_state_t;

    typedef struct
    {
        const char *ssid;
        const char *password;
        int channel;
        int max_connections;
        bool auto_start;
    } wifi_config_ap_settings_t;

    /** Initialize Wi-Fi in Access Point mode with given settings **/
    esp_err_t wifi_config_ap_init(wifi_config_ap_settings_t *settings);

    /** Start the Wi-Fi Access Point **/
    esp_err_t wifi_config_ap_start(void);

    /** Stop the Wi-Fi Access Point **/
    esp_err_t wifi_config_ap_stop(void);

    /** Get current Wi-Fi Access Point state **/
    wifi_config_ap_state_t wifi_config_ap_get_state(void);

    /** Get current netif */
    esp_netif_t *wifi_config_ap_get_netif(void);

#ifdef __cplusplus
}
#endif
