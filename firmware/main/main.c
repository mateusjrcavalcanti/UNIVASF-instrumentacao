#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b)) // Definindo a macro MIN

static const char *TAG_STA = "wifi STA";
static const char *TAG_AP = "wifi AP";
static const char *TAG_HTTP = "wifi STA";
static const char *TAG_NVS = "wifi STA";

#define AP_SSID "LDR"
#define AP_MAX_CONN 4
#define STA_MAX_RETRY 5     // Número máximo de tentativas de conexão
static int s_retry_num = 0; // Contador de tentativas de conexão
static char sta_ssid[32];   // SSID da rede
static char sta_pass[64];   // Senha da rede

wifi_ap_record_t *ap_list = NULL;
uint16_t ap_num = 0;

// Decodificar URL
char *url_decode(const char *src)
{
    char *dst;
    size_t len = strlen(src);
    char *pstr;
    char ch;

    dst = (char *)malloc(len + 1); // Alocar memória para o resultado
    if (dst == NULL)
    {
        return NULL; // Falha na alocação
    }

    pstr = dst;
    while (*src)
    {
        if (*src == '%')
        {
            if (sscanf(src + 1, "%2hhx", &ch) == 1)
            {
                *pstr++ = ch;
                src += 3;
            }
            else
            {
                *pstr++ = *src++;
            }
        }
        else if (*src == '+')
        {
            *pstr++ = ' ';
            src++;
        }
        else
        {
            *pstr++ = *src++;
        }
    }
    *pstr = '\0'; // Finaliza a string

    return dst;
}

// Salvar credenciais na NVS
void save_sta_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_NVS, "Error opening NVS handle!");
        return;
    }

    err = nvs_set_str(nvs_handle, "sta_ssid", ssid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_NVS, "Error setting SSID in NVS");
        nvs_close(nvs_handle);
        return;
    }

    err = nvs_set_str(nvs_handle, "sta_pass", password);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_NVS, "Error setting password in NVS");
        nvs_close(nvs_handle);
        return;
    }

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

// Ler credenciais da NVS
bool load_wifi_credentials(char *ssid, size_t ssid_len, char *password, size_t pass_len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK)
    {
        if (nvs_get_str(nvs_handle, "sta_ssid", ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(nvs_handle, "sta_pass", password, &pass_len) == ESP_OK)
        {
            nvs_close(nvs_handle);
            return true;
        }
        nvs_close(nvs_handle);
    }
    ESP_LOGE(TAG_STA, "Failed to load credentials from NVS");
    return false;
}

void scan_wifi()
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 200,
    };
    esp_wifi_scan_start(&scan_config, false);
}

// Manipulador para a rota /
esp_err_t get_handler(httpd_req_t *req)
{
    char response[1024];
    int len = snprintf(response, sizeof(response),
                       "<html><body>"
                       "<h2>Configure WiFi</h2>"
                       "<form action=\"/save\" method=\"post\">"
                       "<label for=\"ssid\">WiFi Network:</label><br>"
                       "<select name=\"ssid\">");

    for (int i = 0; i < ap_num; i++)
    {
        len += snprintf(response + len, sizeof(response) - len,
                        "<option value=\"%s\">%s</option>", ap_list[i].ssid, ap_list[i].ssid);
    }

    len += snprintf(response + len, sizeof(response) - len,
                    "</select><br><br>"
                    "<label for=\"password\">Password:</label><br>"
                    "<input type=\"password\" name=\"password\"><br><br>"
                    "<input type=\"submit\" value=\"Submit\">"
                    "</form>"
                    "<br>"
                    "<form action=\"/scan\" method=\"post\">"
                    "<input type=\"submit\" value=\"Update WiFi List\">"
                    "</form>"
                    "</body></html>");

    // Verifica se a string foi truncada
    if (len >= sizeof(response))
    {
        ESP_LOGE(TAG_HTTP, "Response buffer too small!");
        return ESP_FAIL;
    }

    httpd_resp_send(req, response, len);
    return ESP_OK;
}

// Manipulador para a rota /save
esp_err_t post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;

        buf[ret] = 0;

        // Decodificar URL
        char *decoded_buf = url_decode(buf);
        if (decoded_buf == NULL)
        {
            ESP_LOGE(TAG_HTTP, "URL Decode failed");
            return ESP_FAIL;
        }

        char *ssid_ptr = strstr(decoded_buf, "ssid=") + 5;
        char *password_ptr = strstr(decoded_buf, "password=") + 9;

        char *ssid_end = strchr(ssid_ptr, '&');
        if (ssid_end)
        {
            *ssid_end = 0;
        }

        strcpy(sta_ssid, ssid_ptr);
        strcpy(sta_pass, password_ptr);

        save_sta_credentials(sta_ssid, sta_pass); // Salvando as credenciais na NVS

        wifi_config_t wifi_sta_config = {
            .sta = {
                .ssid = "",
                .password = "",
            },
        };
        strncpy((char *)wifi_sta_config.sta.ssid, sta_ssid, sizeof(wifi_sta_config.sta.ssid) - 1);
        strncpy((char *)wifi_sta_config.sta.password, sta_pass, sizeof(wifi_sta_config.sta.password) - 1);
        s_retry_num = 0;
        esp_wifi_disconnect();
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
        esp_wifi_connect();

        free(decoded_buf); // Liberar memória alocada para decodificação
    }

    ESP_LOGI(TAG_HTTP, "Received SSID: %s, Password: %s", sta_ssid, sta_pass);

    // Redireciona de volta para a página principal após salvar as credenciais
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

// Manipulador para a rota /scan
esp_err_t scan_handler(httpd_req_t *req)
{
    // Inicia a varredura de redes Wi-Fi
    scan_wifi();

    // Redireciona de volta para a página principal após a varredura
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    
    return ESP_OK;
}

// Inicializa o servidor web
void start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_handler,
        .user_ctx = NULL};

    httpd_uri_t post_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL};

    httpd_uri_t scan_uri = {
        .uri = "/scan",
        .method = HTTP_POST,
        .handler = scan_handler,
        .user_ctx = NULL};

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &get_uri);
        httpd_register_uri_handler(server, &post_uri);
        httpd_register_uri_handler(server, &scan_uri);  // Registrar nova rota /scan
    }
}

// Eventos de WiFi
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            // conecta ao sta
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            if (s_retry_num < STA_MAX_RETRY)
            {
                ESP_LOGI(TAG_STA, "STA disconnected, reconnecting... Attempt %d/%d", s_retry_num + 1, STA_MAX_RETRY);
                esp_wifi_connect();
                s_retry_num++;
            }
            else
            {
                ESP_LOGI(TAG_STA, "Max retry attempts reached (%d). STA will not reconnect automatically.", STA_MAX_RETRY);
            }
            break;
        case WIFI_EVENT_SCAN_DONE:
            esp_wifi_scan_get_ap_num(&ap_num);
            free(ap_list);
            ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_num);
            esp_wifi_scan_get_ap_records(&ap_num, ap_list);

            // Faz o log das informações das redes encontradas
            for (int i = 0; i < ap_num; i++)
            {
                ESP_LOGI(TAG_AP, "SSID: %s, RSSI: %d", (char *)ap_list[i].ssid, ap_list[i].rssi);
            }
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_num = 0; // Resetar o contador ao obter IP
            break;

        default:
            break;
        }
    }
}

// Inicializa o wifi
void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Criar interfaces AP e STA
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configura a conexão STA
    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };
    // Carregar credenciais
    bool creds_loaded = load_wifi_credentials(sta_ssid, sizeof(sta_ssid), sta_pass, sizeof(sta_pass));
    if (creds_loaded)
    {
        strncpy((char *)wifi_sta_config.sta.ssid, sta_ssid, sizeof(wifi_sta_config.sta.ssid) - 1);
        strncpy((char *)wifi_sta_config.sta.password, sta_pass, sizeof(wifi_sta_config.sta.password) - 1);
    }

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            // .channel = AP_CHANNEL,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));                 // Modo AP e STA
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));   // Configura o AP
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config)); // Configura o STA
    ESP_ERROR_CHECK(esp_wifi_start());
    start_webserver();
}

// Função principal
void app_main(void)
{
    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
}