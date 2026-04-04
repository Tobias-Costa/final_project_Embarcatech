#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "ssd1306.h"

#define OLED_SDA_PIN 14
#define OLED_SCL_PIN 15
#define JOYSTICK_VRY_PIN 26

// Configurações do Sistema
#define MAX_ITEMS 20          // Limite da matriz para o Adafruit IO
#define VISIBLE_LINES 4       // Linhas que cabem no display
#define LINE_HEIGHT 10        // Altura de cada linha em pixels

// Estrutura de Dados
typedef struct {
    char name[15];    // Nome do item (ex: "Cafe")
    int quantity;      // Atributo Quantidade
    float price;       // Atributo Preço
} MenuItem;

// Variáveis globais
MenuItem inventory[MAX_ITEMS]; // Matriz de structs
volatile int current_count = 0; // Quantos itens existem no inventário atual
volatile int highlight = 0; // Índice do item selecionado no menu (0 a total)
volatile int shift = 0; // Índice do item que está no topo da tela (scroll)
volatile int joystick_posy; // Posição do eixo Y do joystick em valores decimais(0 a 4095)
volatile bool atualizar_display_flag = true;

void render_menu(ssd1306_t *display){
    ssd1306_clear(display);
    ssd1306_draw_string(display, 5, 5, 1, "Selecione os itens:");
    ssd1306_draw_line(display,5,15,120,15);

    int total_menu_rows = current_count + 1; // Calcula o índice que será usado para escrever "Prosseguir" no display

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
            // Caso contrário, é o índice final: desenha o item fixo "Prosseguir"
            ssd1306_draw_string(display, 0, y_pos, 1, ">>PROSSEGUIR<<");
        }
        // Se este item for o 'highlight', desenhamos um destaque visual (retângulo vazio)
        if (is_selected){
            ssd1306_draw_empty_square(display, 0, y_pos-2, 125, 10);
        }
    }
    ssd1306_show(display);
}

void handle_input(int direction) { 
    

    int total_menu_rows = current_count + 1;

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

void add_item(const char* name, int qty, float price) {
    if (current_count < MAX_ITEMS) {
        strncpy(inventory[current_count].name, name, 15);
        inventory[current_count].quantity = qty;
        inventory[current_count].price = price;
        current_count++;
    }
}

void setup_display_gpios(){
    i2c_init(i2c1, 400000);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
}


void setup_joystick_gpio(){
    adc_init();
    adc_gpio_init(JOYSTICK_VRY_PIN);
    adc_select_input(0);
}

// callback de interrupções
// void gpio_irq_handler_callback(uint gpio, uint32_t events){
//     if (gpio==JOYSTICK_VRY_PIN){
        
//     }
// }

int main()
{
    stdio_init_all();

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

    add_item("Cafe", 10, 5.50);
    add_item("Acucar", 5, 3.20);
    add_item("Leite", 2, 4.80);
    add_item("Pao", 20, 0.50);
    add_item("Manteiga", 1, 9.90);
    
    while (true) {
        joystick_posy = adc_read();
        // 1 = BAIXO, -1 = CIMA
        if (joystick_posy > 3072) // 3° quartil de 4096
            handle_input(-1);
        else if (joystick_posy < 1024) // 1° quartil de 4096
            handle_input(1);
        
        if (atualizar_display_flag){
            atualizar_display_flag = false;
            render_menu(&display);
        }
        sleep_ms(200);
    }
}
