#pragma once
#include "esp_err.h"
#include "esp_netif_ip_addr.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        const char *ssid;     // if NULL => use CONFIG_WIFI_CONN_SSID
        const char *password; // if NULL => use CONFIG_WIFI_CONN_PASSWORD
        int max_retry;        // if < 0 => use CONFIG_WIFI_CONN_MAX_RETRY
        bool auto_start;      // true: auto_start while init
    } wifi_conn_config_t;

    typedef enum
    {
        WIFI_CONN_STATE_IDLE = 0,
        WIFI_CONN_STATE_CONNECTING,
        WIFI_CONN_STATE_GOT_IP,
        WIFI_CONN_STATE_FAILED,
        WIFI_CONN_STATE_DISCONNECTED,
    } wifi_conn_state_t;

    /** Initialize Wi-Fi STA (esp_netif, nvs, wifi driver). */
    esp_err_t wifi_conn_init(const wifi_conn_config_t *cfg);

    /** Start Wi-fi connnection (if haven't start, auto_start = false). */
    esp_err_t wifi_conn_start(void);

    /** Stop Wi-Fi connection. */
    esp_err_t wifi_conn_stop(void);

    /** Return current state. */
    wifi_conn_state_t wifi_conn_get_state(void);

    /** Wait for IP with timeout_ms duration (if timeout_ms < 0 => wait forever). */
    /** Return ESP_OK if have IP */
    esp_err_t wifi_conn_wait_ip(int timeout_ms);

    /** Get current IPv4 (if have). Return true if successful */
    bool wifi_conn_get_ipv4(uint32_t *ip_u32); // network-byte-order

#ifdef __cplusplus
}
#endif
