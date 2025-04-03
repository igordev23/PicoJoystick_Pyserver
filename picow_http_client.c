#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "pico/async_context.h"
#include "lwip/altcp_tls.h"
#include "example_http_client_util.h"

#define HOST "serverpico.onrender.com" // O endereço deve ser sem http:// ou https://
#define URL_REQUEST "/mensagem?msg=" // Não altere essa parte

#define ADC_MAX 4095
#define DEADZONE 500
#define CENTER (ADC_MAX / 2)

#define BUFFER_SIZE 512

// Contador de falhas
int fail_count = 0;

// Função para codificar a URL corretamente
void urlencode(const char *input, char *output, size_t output_size) {
    char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for (size_t i = 0; input[i] && j + 3 < output_size; i++) {
        if ((input[i] >= 'A' && input[i] <= 'Z') || 
            (input[i] >= 'a' && input[i] <= 'z') || 
            (input[i] >= '0' && input[i] <= '9') || 
            input[i] == '-' || input[i] == '_' || input[i] == '.' || input[i] == '~') {
            output[j++] = input[i];
        } else {
            output[j++] = '%';
            output[j++] = hex[(input[i] >> 4) & 0xF];
            output[j++] = hex[input[i] & 0xF];
        }
    }
    output[j] = '\0';
}


// Determina a direção do joystick
const char* get_direction(uint x, uint y) {
    if (x > CENTER + DEADZONE) {
        if (y > CENTER + DEADZONE) return "Nordeste";
        else if (y < CENTER - DEADZONE) return "Noroeste";//
        else return "Norte";//
    } else if (x < CENTER - DEADZONE) {
        if (y > CENTER + DEADZONE) return "Sudeste";//
        else if (y < CENTER - DEADZONE) return "Sudoeste";
        else return "Sul";//
    } else {
        if (y > CENTER + DEADZONE) return "Leste";//
        else if (y < CENTER - DEADZONE) return "Oeste";//
        else return "Centro";
    }
}




// Envia a direção para o servidor
void send_direction(const char* direction) {
    char encoded_dir[BUFFER_SIZE];
    urlencode(direction, encoded_dir, BUFFER_SIZE);

    char full_url[BUFFER_SIZE];
    snprintf(full_url, BUFFER_SIZE, "%s%s", URL_REQUEST, encoded_dir);

    EXAMPLE_HTTP_REQUEST_T req = {0};
    req.hostname = HOST;
    req.url = full_url;
    req.tls_config = altcp_tls_create_config_client(NULL, 0);
    req.headers_fn = http_client_header_print_fn;
    req.recv_fn = http_client_receive_print_fn;

    printf("Enviando direção: %s\n", direction);
    int result = http_client_request_sync(cyw43_arch_async_context(), &req);

    altcp_tls_free_config(req.tls_config);
    req.tls_config = NULL; // Garante que a configuração não seja reutilizada
    if (result != 0) {
    printf("Erro ao enviar a requisição! Código de erro: %d\n", result);
    fail_count++;
    } else {
    fail_count = 0; // Reset da contagem se a requisição for bem-sucedida
}
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // Tempo para inicialização

    adc_init();
    adc_gpio_init(26); // X
    adc_gpio_init(27); // Y

    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha na conexão Wi-Fi\n");
        return 1;
    }

   


    printf("Conectado! Iniciando leitura do joystick...\n");

    while (true) {
        adc_select_input(0);
        uint x = adc_read();

        adc_select_input(1);
        uint y = adc_read();

        const char* direction = get_direction(x, y);
        send_direction(direction);

        sleep_ms(1000); // Aguarda 1 segundo antes de enviar novamente
    }

    cyw43_arch_deinit();
    return 0;
}
