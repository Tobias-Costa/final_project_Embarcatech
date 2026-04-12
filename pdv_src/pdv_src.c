#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "ssd1306.h"
#include "qrcode.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"

// PINOS DO TECLADO MATRICIAL
// Linhas
#define ROWS 4
uint row_pins[ROWS] = {16, 17, 18, 19};

// Colunas
#define COLUMNS 4
uint columns_pins[COLUMNS] = {20, 4, 9, 8};

char keymap[ROWS][COLUMNS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

// PERIFÉRICOS

// Display
#define OLED_SDA_PIN 14
#define OLED_SCL_PIN 15

// Joystick
#define JOYSTICK_VRY_PIN 26

// Botões
#define BTN_A 5
#define BTN_B 6

// Buzzer
#define BUZZER_PIN 10

// CONFIGURAÇÕES DO SISTEMA
#define MAX_ITEMS 20          // Limite da matriz
#define VISIBLE_LINES 4       // Linhas que cabem no display
#define LINE_HEIGHT 10        // Altura de cada linha em pixels

// CONSTANTES DE CONEXÃO
#define WIFI_SSID "NOME-DA-REDE"
#define WIFI_PASSWORD "SENHA-DA-REDE"
#define MQTT_SERVER "mqtt.thingsboard.cloud"
#define THINGSBOARD_TOKEN "TOKEN-DO-DEVICE"
#define TELEMETRY_TOPIC "v1/devices/me/telemetry"

// ESTRUTURA DE DADOS
typedef struct {
    char name[15];    // Nome do item (ex: "Cafe")
    int quantity;      // Atributo Quantidade
    float price;       // Atributo Preço
} MenuItem;

// VARIÁVEIS GLOBAIS
MenuItem inventory[MAX_ITEMS]; // Matriz de structs
volatile int frame = 0; // Representa a tela em que o usuário está
volatile bool mqtt_connected = false; // Booleano que indica se está conectado ao broker mqtt
volatile bool send_sale_flag = false; // Indica se uma venda foi finalizada e está pronta para envio
volatile bool buzzer_flag = false; // Sinaliza quando um sinal sonoro deve ser reproduzido
int current_count = 0; // Quantos itens existem no inventário atual
int highlight = 0; // Índice do item selecionado no menu (0 a total)
int shift = 0; // Índice do item que está no topo da tela (scroll)
int joystick_posy; // Posição do eixo Y do joystick em valores decimais(0 a 4095)
float total_bill = 0; // Variável que armazena valor total a se pagar pelos produtos
float input_value = 0; // Variável que armazena o valor pago pelo cliente
float change_value = 0; // Variável que armazena valor do troco
char string_pix_buffer[512]; // Armazena o BR code do pix
char pix_key[256] = "SUA-CHAVE-PIX"; // Armazena string da chave pix(email,cpf,telefone,cnpj)
bool atualizar_display_flag = true; // Flag que sinaliza para o display ser atualizado

mqtt_client_t *static_client;
struct mqtt_connect_client_info_t ci;
uint buzzer_slice;

// FUNÇÕES DE CONEXÃO WIFI E MQTT

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) { // Callback quando a conexão MQTT muda de estado
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("Conectado ao ThingsBoard!\n");
        mqtt_connected = true;
    } else {
        printf("Falha na conexão MQTT: %d\n", status);
        mqtt_connected = false;
    }
}

void send_telemetry(mqtt_client_t *client, const char *payload) { // Função para enviar dados (Telemetria)
    if (mqtt_client_is_connected(client)) {
        cyw43_arch_lwip_begin();
        mqtt_publish(client, TELEMETRY_TOPIC, payload, strlen(payload), 1, 0, NULL, NULL);
        cyw43_arch_lwip_end();
        printf("Dados enviados: %s\n", payload);
    }
}

static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *arg) { // Função responsável por resolver o IP do ThingsBoard CLoud
    if (ipaddr) {
        printf("DNS resolvido!\n");

        cyw43_arch_lwip_begin();
        mqtt_client_connect(static_client, ipaddr, 1883, mqtt_connection_cb, NULL, &ci);
        cyw43_arch_lwip_end();

    } else {
        printf("Falha no DNS\n");
    }
}

void setup_wifi(){ // Função responsável por configurar a conexão wifi
    // Inicializa a arquitetura do chip Wi-Fi CYW43
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        return;
    }

    // Ativa o modo Estação (STA) para se conectar a um roteador
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi %s...\n", WIFI_SSID);

    // Tenta conectar com um timeout de 30 segundos
    int resultado = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, 
        WIFI_PASSWORD, 
        CYW43_AUTH_WPA2_AES_PSK, 
        30000
    );

    if (resultado != 0) {
        printf("Erro ao conectar: %d\n", resultado);
    } else {
        printf("Conectado com sucesso!\n");
    }

}

void send_sale_mqtt(mqtt_client_t *client){ // Prepara os dados de vendas e envia uma string para a função send_telemetry() para em seguida enviar ao servidor via mqtt
    char payload[512];
    char items[300] = "";
    bool first_item = true;

    for (int i = 0; i < current_count; i++) {
            
            if (!first_item){
                strcat(items, ",");
            }
            first_item = false;

            char item[64];
            snprintf(item, sizeof(item),
                "\"%s\": %d",
                inventory[i].name,
                inventory[i].quantity,
                inventory[i].price
            );
            strcat(items, item);
    
    }

    snprintf(payload, sizeof(payload),
        "{\"total\": %.2f, %s}",
        total_bill,
        items
    );

    send_telemetry(client, payload);

}

// FUNÇÕES DE GERAÇÃO DE BR CODE

uint16_t crc16_ccitt_calculation(char* data) { // Função para calcular o CRC16 [Importante para gera uma string BR code (Polinômio 0x1021)] 
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < strlen(data); i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc & 0xFFFF;
}

void generate_pix_string(char* buffer, char* key, float value) { // Gera uma string BR code para pagamentos em pix
    char payload[256]; 
    char field26[100]; // Campo da string que armazena os dados do destinatário para o correto envio do pix
    char value_string[15]; // Armazena a string do valor do pix
    
    sprintf(value_string, "%.2f", value); // Transforma o float 'value' em uma string com 2 casas decimais(ex: 20.26)
    
    // Filtro para remover caracteres indesejados
    char clean_key[80];
    bool is_email = (strchr(key, '@') != NULL); // Verifica se existe '@' na chave
    int j=0;

    for (int i=0; key[i]; i++){
        if (isalnum(key[i]) || key[i]=='@' || key[i]=='+' || (key[i] == '.' && is_email))
            clean_key[j++] = key[i];
    }
    clean_key[j] = '\0'; //Fecha a string

    // Monta o campo 26: 0014br.gov.bcb.pix + 01 + tamanho da chave(sempre 2 dígitos) + chave
    sprintf(field26, "0014BR.GOV.BCB.PIX01%02d%s", (int)strlen(clean_key), clean_key);

    // Monta estrutura do BR Code: 00(02)01 | 26(tamanho)dados | 52(04)0000 | 53(03)986 | 54(tamanho)valor | 58(02)BR | 59(01)N | 60(01)C | 62(07)0503*** | 63(04)
    sprintf(payload, "00020126%02d%s52040000530398654%02d%s5802BR5901N6001C62070503***6304", 
            (int)strlen(field26), field26, (int)strlen(value_string), value_string);

    // Calcula CRC e anexa ao final
    uint16_t crc = crc16_ccitt_calculation(payload);
    sprintf(buffer, "%s%04X", payload, crc);
}

// FUNÇÕES DE DISPLAY

int get_total_menu_rows(){ // Retorna quantos itens deverá haver no menu principal.
    if (total_bill>0){
        return current_count + 1; // Calcula o índice que será usado para escrever os itens e o selecionável "Prosseguir" no display
    } else{
        return current_count; // Calcula o índice que mostrará apenas os itens.
    }
}

void render_frame_zero(ssd1306_t *display){
    // Desenha cabeçalho
    ssd1306_draw_string(display, 5, 5, 1, "Selecione os itens:");
    ssd1306_draw_line(display,5,15,120,15);

    // Caso o usuário tenha selecionado pelo menos um produto, aparecerá o selecionável 'Prosseguir'. Caso contrário, não é desenhado.
    int total_menu_rows = get_total_menu_rows();

    for (int i = 0; i < VISIBLE_LINES; i++) {
        // item_index mapeia a linha física da tela para a posição real na matriz
        int item_index = i + shift;

        // Condicional: Se o índice ultrapassar o total de opções, paramos de desenhar
        if (item_index >= total_menu_rows) {
            break;
        }

        int y_pos = i * LINE_HEIGHT + 20;
        bool is_selected = (item_index == highlight);

        // Condicional: Verifica se estamos desenhando um item da matriz ou o "Prosseguir"
        if (item_index < current_count) {
            ssd1306_draw_string(display, 5, y_pos, 1, inventory[item_index].name);
            char qty_buffer[10]; 
            snprintf(qty_buffer, sizeof(qty_buffer), "%dx", inventory[item_index].quantity); // snprintf: (onde salvar, tamanho, formato, variável)
            ssd1306_draw_string(display, 105, y_pos, 1, qty_buffer);
        } else{
            // Caso os itens da matriz 'inventory' tenham acabado e o usuário seleciona um produto: desenha o item fixo "Prosseguir"
            ssd1306_draw_string(display, 0, y_pos, 1, ">>PROSSEGUIR<<");
        }
        // Se este item for o 'highlight', desenhamos um destaque visual (retângulo vazio)
        if (is_selected){
            ssd1306_draw_empty_square(display, 0, y_pos-2, 125, 10);
        }
    }
}

void render_frame_one(ssd1306_t *display){
    // Desenha Cabeçalho
    ssd1306_draw_string(display, 35, 5, 1, "Pagamento:");
    ssd1306_draw_line(display,5,15,120,15);
    // Imprime no display o valor total dos itens escolhidos
    char bill_buffer[20];
    snprintf(bill_buffer, sizeof(bill_buffer), "Total: R$%.2f", total_bill); // snprintf: (onde salvar, tamanho, formato, variável)
    ssd1306_draw_string(display, 5, 20, 1, bill_buffer);
    
    // Área de seleção de forma de pagamento(pix ou dinheiro)
    ssd1306_draw_string(display, 5, 35, 1, "Forma de pagamento:");
    ssd1306_draw_empty_square(display, 0, 50, 64, 12);
    ssd1306_draw_empty_square(display, 64, 50, 62, 12);
    ssd1306_draw_string(display, 10, 53, 1, "Dinheiro");
    ssd1306_draw_string(display, 85, 53, 1, "Pix");
}

void render_frame_two(ssd1306_t *display){
    // Desenha Cabeçalho
    ssd1306_draw_string(display, 20, 5, 1, "Digite o valor:");
    ssd1306_draw_line(display,5,15,120,15);
    // Imprime no display o valor total dos itens escolhidos
    char bill_buffer[20];
    snprintf(bill_buffer, sizeof(bill_buffer), "Total: R$%.2f", total_bill); // snprintf: (onde salvar, tamanho, formato, variável)
    ssd1306_draw_string(display, 5, 20, 1, bill_buffer);
    // Imprime entrada de valor digitado e o troco(entrada - total)
    char input_buffer[20];
    snprintf(input_buffer, sizeof(input_buffer), "Entrada: R$%.2f", input_value); // snprintf: (onde salvar, tamanho, formato, variável)
    ssd1306_draw_string(display, 5, 30, 1, input_buffer);

    char change_value_buffer[20];
    snprintf(change_value_buffer, sizeof(change_value_buffer), "Troco: R$%.2f", change_value); // snprintf: (onde salvar, tamanho, formato, variável)
    ssd1306_draw_string(display, 5, 40, 1, change_value_buffer);

    ssd1306_draw_empty_square(display, 0, 50, 64, 12);
    ssd1306_draw_empty_square(display, 64, 50, 62, 12);
    ssd1306_draw_string(display, 10, 53, 1, "Cancelar");
    ssd1306_draw_string(display, 70, 53, 1, "Concluir");

}

void render_frame_three(ssd1306_t *display){
    // Inicializa e cria QRCODE a partir da string do BRcode pix
    QRCode qrcode;
    uint8_t qrcodeBytes[qrcode_getBufferSize(11)]; // Versão 11 (61x61 pixels)
    qrcode_initText(&qrcode, qrcodeBytes, 11, ECC_QUARTILE, string_pix_buffer);
    // Define uma margem(offset) para centralizar o qrcode
    int offset_x = 60;
    int offset_y = 2;
    // Desenha QRCODE
    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                ssd1306_draw_pixel(display, (x)+offset_x, (y)+offset_y);
                ssd1306_invert(display, 1);
            }
        }
    }
    ssd1306_draw_empty_square(display, 2, 5, 55, 25);
    ssd1306_draw_empty_square(display, 2, 36, 55, 25);
    ssd1306_draw_string(display, 5, 10, 1, "Cancelar");
    ssd1306_draw_string(display, 5, 20, 1, ">Botao A");
    ssd1306_draw_string(display, 5, 40, 1, "Concluir");
    ssd1306_draw_string(display, 5, 50, 1, ">Botao B");
}

void render_display(ssd1306_t *display){ // Renderiza os frames da aplicação no display
    ssd1306_clear(display);
    // Torna a tela novamente ao padrão
    ssd1306_invert(display, 0);

    if (frame==0){
        render_frame_zero(display);     
    }else if (frame==1){
        render_frame_one(display);
    }else if (frame==2){
        render_frame_two(display);
    } else if (frame==3){
        // Cria BRCODE
        generate_pix_string(string_pix_buffer, pix_key, total_bill);
        render_frame_three(display);
    }

    ssd1306_show(display);
}

void restart_menu(){ // Reinicia os atributos de quantity em inventory e coloca highlight e shift iguais a 0.
    frame = 0;
    highlight = 0;
    shift = 0;
    input_value = 0;
    for(int i=0; i < current_count; i++){
        inventory[i].quantity = 0;
    }
}

// FUNÇÕES DE TRATAMENTO DE PERIFÉRICOS

void handle_input(int direction) { // A partir da direção do joystick atualiza as variáveis highlight, shift e atualizar_display_flag
    
    // Caso o usuário tenha selecionado pelo menos um produto, aparecerá o selecionável 'Prosseguir'. Caso contrário, não é desenhado.
    int total_menu_rows = get_total_menu_rows();

    // Atualiza o destaque
    highlight += direction;

    // Condicional de Limite Superior: Não deixa subir além do primeiro item
    if (highlight < 0) {
        highlight = 0;
    }
    // Condicional de Limite Inferior: Não deixa descer além do "Prosseguir"
    if (highlight >= total_menu_rows) {
        highlight = total_menu_rows - 1;
    }

    // --- LÓGICA DE SCROLL (A Janela) ---

    // Condicional: Se o destaque subiu acima da janela visível
    if (highlight < shift) {
        shift = highlight;
    }
    // Condicional: Se o destaque desceu abaixo da borda inferior da janela
    else if (highlight >= shift + VISIBLE_LINES) {
        shift = highlight - VISIBLE_LINES + 1;
    }

    atualizar_display_flag = true;
}

void buzzer_tone(int freq, int duration_ms) {
    uint32_t clock = 125000000; // clock da Pico
    uint32_t top = clock / freq - 1;
    pwm_set_wrap(buzzer_slice, top);
    pwm_set_gpio_level(BUZZER_PIN, top / 2); // 50% duty
    sleep_ms(duration_ms);
    pwm_set_gpio_level(BUZZER_PIN, 0); // desliga
}

char read_keyboard(){ // Função responsável por ler os caracteres do teclado matricial
    for (int i = 0; i < ROWS; i++){
        gpio_put(row_pins[i], 1);
        sleep_ms(10); // Delay para estabilização elétrica

        for (int j = 0; j < COLUMNS; j++){
            if (gpio_get(columns_pins[j])){
                sleep_ms(50); // Debounce
                if (gpio_get(columns_pins[j])){
                    buzzer_tone(2500,100);
                    while(gpio_get(columns_pins[j])){ // Trava enquanto usuário estiver segurando o botão
                        sleep_ms(10);
                    } 
                    gpio_put(row_pins[i], 0);
                    return keymap[i][j];
                }
            }
        }
        gpio_put(row_pins[i], 0); // Desativa
    }
    return '\0'; // Nenhuma tecla pressionada
}

// FUNÇÕES DE INTERRUPÇÕES

int64_t debounce_alarm_timer_callback(alarm_id_t id, void *user_data){ // Função callback para o debounce de alarme de reativação das interrupções dos botôes A e B
    int gpio = (int)(intptr_t)user_data;
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, true);
    return 0;
}

void gpio_irq_handler_callback(uint gpio, uint32_t events){ // Callback que trata as interrupções dos botões
    if(frame==0){
        // INTERRUPÇÕES FRAME 0
    
        // Incrementa o item selecionado
        if (gpio==BTN_B && highlight < current_count){
            gpio_set_irq_enabled(BTN_B, GPIO_IRQ_EDGE_FALL, false);
            inventory[highlight].quantity++;   
        }
        
        // Decrementa o item selecionado
        if (gpio==BTN_A && highlight < current_count){
            gpio_set_irq_enabled(BTN_A, GPIO_IRQ_EDGE_FALL, false);
            if (inventory[highlight].quantity > 0)
                inventory[highlight].quantity--;   
        }
    
        // Se o cursor tiver selecionado ">>Prosseguir<<" o display irá para o próximo frame
        if (highlight==current_count){
            gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, false);
            frame = 1;
        }
    } else if (frame==1){
        // INTERRUPÇÕES FRAME 1
    
        // Seleciona a forma de pagamento (Dinheiro=>BTNA; Pix=>BTNB)
        if (gpio==BTN_A){
            gpio_set_irq_enabled(BTN_A, GPIO_IRQ_EDGE_FALL, false);
            frame = 2;
        } else if (gpio==BTN_B){
            gpio_set_irq_enabled(BTN_B, GPIO_IRQ_EDGE_FALL, false);
            frame = 3;  
        }
    }else if (frame==2 || frame == 3){
        // INTERRUPÇÕES FRAME 2 E 3

        if (gpio==BTN_A){ //CANCELAR resultará no cancelamento da compra e irá limpar os itens escolhidos em 'inventory'.
            gpio_set_irq_enabled(BTN_A, GPIO_IRQ_EDGE_FALL, false);
            restart_menu();
        } else if (gpio==BTN_B){ // CONLUIR resultará na conclusão da comprar e irá limpar os itens escolhidos em 'inventory' para uma nova compra
            // ADICIONAR VALIDAÇÃO COM BUZZER PARA QUANDO CHANGE_VALUE FOR >= 0 E PARA QUANDO NÃO ATENDER A CONDIÇÃO
            gpio_set_irq_enabled(BTN_B, GPIO_IRQ_EDGE_FALL, false);
            if (change_value>=0 && frame==2){
                send_sale_flag = true;
            }else if (frame==3){
                send_sale_flag = true;
            }
        }
    }

    // Atualiza a tela e ativa o alarme de debounce
    atualizar_display_flag = true;
    buzzer_flag = true;
    add_alarm_in_ms(200, debounce_alarm_timer_callback, (void *)(intptr_t)gpio, false);     
}

// FUNÇÃO DE ADICIONAR ITENS
void add_item(const char* name, int qty, float price) { // Trata e adiciona os dados na struct principal(inventory)
    if (current_count < MAX_ITEMS) {
        strncpy(inventory[current_count].name, name, 15);
        inventory[current_count].quantity = qty;
        inventory[current_count].price = price;
        current_count++;
    }
}

// FUNÇÕES DE SETUP DE PERIFÉRIOCS

void setup_buzzer(){ // Inicializa o buzzer e configura-o para usar PWM
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // ajusta frequência base
    pwm_set_enabled(buzzer_slice, true);
}

void setup_btn_gpios(){ // Inicializa e configura os pinos dos botões A e B
    gpio_init(BTN_A); gpio_set_dir(BTN_A,GPIO_IN); gpio_pull_up(BTN_A);
    gpio_init(BTN_B); gpio_set_dir(BTN_B, GPIO_IN); gpio_pull_up(BTN_B);
}

void setup_display_gpios(){ // Inicializa e configura os pinos do display
    i2c_init(i2c1, 400000);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
}

void setup_joystick_gpio(){ // Inicializa e configura o pino do joystick do eixo Y
    adc_init();
    adc_gpio_init(JOYSTICK_VRY_PIN);
    adc_select_input(0);
}

void setup_matrix_keyboard_gpios(){ // Inicializa e configura os pinos do teclado matricial
    // Configura linhas como saída e inicialmente em low
    for (int i = 0; i < ROWS; i++){
        gpio_init(row_pins[i]);
        gpio_set_dir(row_pins[i], GPIO_OUT);
        gpio_put(row_pins[i], 0);
    }
    // Configura colunas como entrada e em pull_dowm
    for (int i = 0; i < COLUMNS; i++){
        gpio_init(columns_pins[i]);
        gpio_set_dir(columns_pins[i], GPIO_IN);
        gpio_pull_down(columns_pins[i]);
    }
}

int main()
{
    // Inicializa módulo stdio
    stdio_init_all();

    // Configurando botões A e B
    setup_btn_gpios();

    // Configurando o display OLED
    setup_display_gpios();
    ssd1306_t display;
    display.external_vcc=false;
    ssd1306_init(&display,
        128, // Comprimento
        64, // Largura
        0x3C, // Endereço
        i2c1); // Canal I2C

    // Configurando o joystick
    setup_joystick_gpio();

    // Configura teclado matricial
    setup_matrix_keyboard_gpios();
    
    // Configura buzzer
    setup_buzzer();

    // Configura wifi
    setup_wifi();

    // Configuração do Cliente MQTT
    static_client = mqtt_client_new();
    memset(&ci, 0, sizeof(ci));
    ci.client_id = "PicoW_Device";
    ci.client_user = THINGSBOARD_TOKEN; //TOKEN do Device no Thingsboard
    ci.client_pass = NULL;              // Senha vazia
    ci.keep_alive = 60;

    // Uso de DNS para buscar IP do Broker mqtt
    ip_addr_t mqtt_server_addr;
    err_t err = dns_gethostbyname(MQTT_SERVER, &mqtt_server_addr, dns_callback, NULL);

    // Valida mensagem retornada pelo dns
    if (err == ERR_OK) {
        printf("DNS resolvido imediatamente\n");

        cyw43_arch_lwip_begin();
        mqtt_client_connect(static_client, &mqtt_server_addr, 1883, mqtt_connection_cb, NULL, &ci);
        cyw43_arch_lwip_end();
    }
    else if (err == ERR_INPROGRESS) {
        printf("Aguardando DNS...\n");
    }
    else {
        printf("Erro DNS: %d\n", err);
    }
    
    // Adiciona itens, quantidades iniciais(0) e preço unitário
    add_item("Cafe", 0, 5.50);
    add_item("Acucar", 0, 3.20);
    add_item("Leite", 0, 4.80);
    add_item("Pao", 0, 0.50);
    add_item("Manteiga", 0, 9.90);
    add_item("Bolo", 0, 5.00);
    add_item("Refrigerante", 0, 3.50);

    // Funções de interrupção
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler_callback);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler_callback);

    while (true) {
        // Condicional adiciona para evitar leituras analógicas sem necessidade
        if (frame==0){

            joystick_posy = adc_read();
            // Aciona handle_input e envia os argumento com base nas seguintes condições: 1 = BAIXO, -1 = CIMA
            if (joystick_posy > 3072) // 3° quartil de 4096
                handle_input(-1);
            else if (joystick_posy < 1024) // 1° quartil de 4096
                handle_input(1);

        } else if (frame==2){

            char tecla = read_keyboard();
            if (tecla != '\0'){
                // Se for um dígito entre 0 a 9
                atualizar_display_flag = true;
                if (tecla >= '0'  && tecla <= '9'){
                    int digito = tecla - '0';
                    // Valor cresce iniciando nas casas decimais
                    input_value = ((input_value * 1000.0) + digito)/100.0;
                } else if (tecla == 'D'){ // Reseta input_value (Clear)
                    input_value = 0.0;
                }
            }
        }
        
        // Se a flag for atualizada para true, o diplay é renderizado novamente
        if (atualizar_display_flag){
            // Calcula valor total dos itens escolhidos
            total_bill = 0;
            for(int i=0; i < current_count; i++){
                total_bill += inventory[i].price * inventory[i].quantity;
            }

            //  Cálculo do troco
            change_value = input_value - total_bill;
            // Correção de precisão para eliminar o "-0.00"
            if (change_value < 0 && change_value > -0.001) {
                change_value = 0.0;
            }

            // Desativa flag para que não ocorra atualizações de display desnecessária
            atualizar_display_flag = false;
            render_display(&display);
        }

        if (send_sale_flag && mqtt_connected){
            send_sale_flag=false;
            send_sale_mqtt(static_client);
            restart_menu();
            atualizar_display_flag = true;
        }

        if (buzzer_flag){
            buzzer_flag=false;
            buzzer_tone(2500, 100);
        }

        sleep_ms(200);
    }
}
