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
#include "driver/adc.h"
#include "esp_websocket_client.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b)) // Definindo a macro MIN

static const char *TAG_STA = "STA";
static const char *TAG_AP = "AP";
static const char *TAG_HTTP = "HTTP";
static const char *TAG_NVS = "NVS";
static const char *TAG_ADC = "ADC";
static const char *TAG_WEBSOCKET = "WEBSOCKET";

#define ADC_CHANNEL ADC1_CHANNEL_4 // Canal ADC1 conectado ao LDR
#define ADC_WIDTH ADC_WIDTH_BIT_12 // Resolução de 12 bits
#define ADC_ATTEN ADC_ATTEN_DB_12
#define RESISTOR_VALUE 1000 // Resistor de 1kΩ em série com o LDR

#define AP_SSID "LDR"
#define AP_MAX_CONN 4
#define STA_MAX_RETRY 5     // Número máximo de tentativas de conexão
static int s_retry_num = 0; // Contador de tentativas de conexão
static char sta_ssid[32];   // SSID da rede
static char sta_pass[64];   // Senha da rede

wifi_ap_record_t *ap_list = NULL;
uint16_t ap_num = 0;

#define WS_URL "ws://192.168.18.100" // URL do servidor WebSocket
#define WS_PORT 3000                 // Porta do servidor WebSocket

esp_websocket_client_handle_t websocket_client = NULL;
SemaphoreHandle_t websocket_mutex = NULL;

// Manipulador de eventos do WebSocket
void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG_WEBSOCKET, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_WEBSOCKET, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG_WEBSOCKET, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG_WEBSOCKET, "Received opcode=%d", data->op_code);
        ESP_LOGW(TAG_WEBSOCKET, "Received=%.*s", data->data_len, (char *)data->data_ptr);
        ESP_LOGW(TAG_WEBSOCKET, "Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG_WEBSOCKET, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

// Inicializa o WebSocket
void websocket_start()
{
    esp_websocket_client_config_t websocket_cfg = {
        .uri = WS_URL,
        .port = WS_PORT,
        // .skip_cert_common_name_check = true,
    };

    ESP_LOGI(TAG_WEBSOCKET, "Connecting to %s...", websocket_cfg.uri);

    websocket_client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(websocket_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)websocket_client);

    websocket_mutex = xSemaphoreCreateMutex();

    esp_websocket_client_start(websocket_client);

    esp_event_handler_register(WEBSOCKET_EVENTS, WEBSOCKET_EVENT_ANY, &websocket_event_handler, websocket_client);
}

// Encerra o WebSocket
void websocket_stop()
{
    esp_websocket_client_stop(websocket_client);
    esp_websocket_client_destroy(websocket_client);
}

// Envia uma mensagem via WebSocket
void websocket_send_message(const char *message)
{
    if (websocket_client == NULL)
    {
        ESP_LOGE(TAG_WEBSOCKET, "WebSocket client not initialized");
        return;
    }

    // Adquire o mutex antes de enviar dados
    if (xSemaphoreTake(websocket_mutex, portMAX_DELAY) == pdTRUE)
    {
        int len = strlen(message);
        if (esp_websocket_client_is_connected(websocket_client))
        {
            ESP_LOGI(TAG_WEBSOCKET, "Sending message: %s", message);
            esp_websocket_client_send_text(websocket_client, message, len, portMAX_DELAY);
        }
        else
        {
            ESP_LOGE(TAG_WEBSOCKET, "WebSocket client is not connected");
        }
        xSemaphoreGive(websocket_mutex); // Libere o mutex após o envio
    }
}

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
        httpd_register_uri_handler(server, &scan_uri); // Registrar nova rota /scan
    }
}

// Manipulador de eventos de WiFi
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
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
        if (esp_websocket_client_is_connected(websocket_client))
        {
            websocket_stop();
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
    case IP_EVENT_STA_GOT_IP:
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0; // Resetar o contador ao obter IP
        websocket_start();
        break;

    default:
        break;
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

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));  // Registra um manipulador de eventos para o evento Wi-Fi
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL)); // Registra um manipulador de eventos para o evento IP

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

void ldr_task(void *pvParameter)
{
    // Configura o canal ADC
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    while (1)
    {
        // Realiza a leitura do ADC
        int adc_reading = adc1_get_raw(ADC_CHANNEL);
        // Converte a leitura ADC para a tensão correspondente (V)
        float voltage = (adc_reading / 4095.0) * 3.6; // Para uma resolução de 12 bits (0-4095) e 3.6V
        // Calcula a resistência do LDR usando o divisor de tensão
        float ldr_resistance = (voltage * RESISTOR_VALUE) / (3.6 - voltage);

        ESP_LOGI(TAG_ADC, "Leitura ADC: %d | Tensão lida: %.2fV | Resistência LDR: %.2f Ohms",
                 adc_reading,
                 voltage,
                 ldr_resistance);

        if (esp_websocket_client_is_connected(websocket_client))
        {
            char message[128];
            snprintf(message, sizeof(message),
                     "Leitura ADC: %d | Tensão lida: %.2fV | Resistência LDR: %.2f Ohms",
                     adc_reading, voltage, ldr_resistance);

            websocket_send_message(message);
        }

        // Adiciona um atraso de 1 segundo
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
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

    // Cria a task para realizar a leitura do LDR
    xTaskCreate(&ldr_task, "ldr_task", 2048, NULL, 5, NULL);
}
