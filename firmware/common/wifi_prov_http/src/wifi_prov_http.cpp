#include <stdlib.h>
#include <string.h>

#include "wifi_prov_http.h"

#include "esp_log.h"
#include "esp_http_server.h"
#include "wifi_nvs.h"

static const char *TAG = "wifi_prov_http";

static httpd_handle_t server_handle = NULL;
static wifi_prov_http_handle_save_t save_handler = NULL;
static void *save_handler_user_ctx = NULL;

/* ---------- Utils ---------- */
// Decode a single hex character to its integer value
static int hexval(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// URL-decode a string in place
static size_t url_decode(char *s)
{
    char *o = s, *p = s;
    while (*p)
    {
        if (*p == '%')
        {
            int h1 = hexval(*(p + 1)), h2 = hexval(*(p + 2));
            if (h1 >= 0 && h2 >= 0)
            {
                *o++ = (char)((h1 << 4) | h2);
                p += 3;
            }
            else
            {
                *o++ = *p++;
            }
        }
        else if (*p == '+')
        {
            *o++ = ' ';
            p++;
        }
        else
        {
            *o++ = *p++;
        }
    }
    *o = 0;
    return (size_t)(o - s);
}

// Extract the value for a given key from URL-encoded form data
static void form_get_kv(const char *body, const char *key, char *out, size_t outlen)
{
    if (!out || outlen == 0)
    {
        return;
    }
    out[0] = 0;
    const size_t klen = strlen(key);
    const char *p = body;
    while (p && *p)
    {
        const char *eq = strchr(p, '=');
        if (!eq)
            break;
        if ((size_t)(eq - p) == klen && strncmp(p, key, klen) == 0)
        {
            const char *val = eq + 1;
            const char *amp = strchr(val, '&');
            size_t len = amp ? (size_t)(amp - val) : strlen(val);
            if (len >= outlen)
                len = outlen - 1;
            memcpy(out, val, len);
            out[len] = 0;
            url_decode(out);
            return;
        }
        const char *amp = strchr(p, '&');
        p = amp ? amp + 1 : NULL;
    }
}

// Validate the lengths of SSID and password
static bool creds_len_valid(const char *ssid, const char *pass)
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

/* ---------- HTTP Handlers ---------- */
// Handler for the root path, serves the HTML form
static esp_err_t root_get_handler(httpd_req_t *req)
{
    static const char html[] = R"rawliteral(
        <!doctype html>
        <html>
            <head>
                <meta charset="utf-8"/>
                <meta name="viewport" content="width=device-width,initial-scale=1"/>
                <title>Wi-Fi Provision</title>
                <style>
                    body { font-family: system-ui, Segoe UI, Arial; margin: 2rem; }
                    label { display: block; margin-top: 1rem; }
                    input { padding: .6rem; width: 22rem; max-width: 100%; }
                    button { margin-top: 1rem; padding: .6rem 1rem; }
                </style>
            </head>
            <body>
                <h2>Configure Wi-Fi</h2>
                <form method="POST" action="/save">
                    <label>SSID</label>
                    <input name="ssid" required maxlength="32">
                    <label>Password</label>
                    <input name="pass" type="password" maxlength="64">
                    <button type="submit">Save</button>
                </form>
            </body>
        </html>
    )rawliteral";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, sizeof(html) - 1);
}

// Saved credentials callback and context
static void deferred_save_callback(const char *ssid, const char *pass)
{
    ESP_LOGI(TAG, "Saving Wi-Fi credentials: SSID='%s', PASS='%s'", ssid, pass);
    // Call the registered save handler if available
    if (save_handler)
    {
        save_handler(ssid, pass, save_handler_user_ctx);
    }
}

// Handler for saving Wi-Fi credentials
static esp_err_t save_post_handler(httpd_req_t *req)
{
    const size_t max_content_len = 512;
    if (req->content_len <= 0 || req->content_len > max_content_len)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad content size");
    }

    // Read the request body
    char *buf = (char *)malloc(req->content_len + 1);
    if (!buf)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");

    int got = 0;
    while (got < req->content_len)
    {
        int r = httpd_req_recv(req, buf + got, req->content_len - got);
        if (r <= 0)
        {
            free(buf);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
        }
        got += r;
    }
    buf[got] = 0;

    char ssid[33], pass[65];
    form_get_kv(buf, "ssid", ssid, sizeof(ssid));
    form_get_kv(buf, "pass", pass, sizeof(pass));
    free(buf);

    if (!creds_len_valid(ssid, pass))
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ssid/pass length");
    }

    esp_err_t e = wifi_nvs_set(ssid, pass);
    if (e != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }

    static const char ok[] = R"rawliteral(
        <!doctype html>
        <html>
            <body>
                <h3>Saved.</h3>
                <p>You can disconnect from this AP now.</p>
            </body>
        </html>
    )rawliteral";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, ok, sizeof(ok) - 1);

    /* Gọi callback (nếu có) sau khi đã gửi response */
    deferred_log_and_cb(ssid, pass);
    return ESP_OK;
}

/* ---------- Public API ---------- */
void wifi_prov_http_register_save_handler(wifi_prov_http_handle_save_t save_handler_cb, void *user_ctx)
{
    save_handler = save_handler_cb;
    save_handler_user_ctx = user_ctx;
}

esp_err_t wifi_prov_http_init(void)
{
    // Check if server is already running
    if (server_handle != NULL)
    {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }

    // Start the HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    // Start the server
    esp_err_t e = httpd_start(&server_handle, &config);
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(e));
        return e;
    }

    // Register URI handlers
    httpd_uri_t root_get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server_handle, &root_get_uri);

    // Register the save handler
    httpd_uri_t save_post_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server_handle, &save_post_uri);

    ESP_LOGI(TAG, "Wi-Fi provisioning HTTP server started on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t wifi_prov_http_deinit(void)
{
    // Check if server is running
    if (server_handle == NULL)
    {
        ESP_LOGW(TAG, "HTTP server not running");
        return ESP_OK;
    }

    // Stop the HTTP server
    esp_err_t e = httpd_stop(server_handle);
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(e));
        return e;
    }

    server_handle = NULL;
    ESP_LOGI(TAG, "Wi-Fi provisioning HTTP server stopped");
    return ESP_OK;
}