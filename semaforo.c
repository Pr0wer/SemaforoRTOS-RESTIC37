#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "lib/ws2812b.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include <stdio.h>

// Pinos
const uint led_red_pin = 13;
const uint led_green_pin = 11;
const uint buzzer_pin = 21;
const uint btn_a_pin = 5;

// Codificação dos valores dos sinais do semáforo
#define VERDE 0
#define AMARELO 1
#define VERMELHO 2


uint8_t sinal_atual = VERDE; // Cor inicial do semáforo
uint8_t sinal_anterior = VERDE; // Buffer da cor do sinal anterior para desligamento do LED
const uint8_t sinal_noturno = AMARELO; // Cor do sinal no modo noturno


// Intervalo entre os sinais
#define INTERVALO 10
#define AMARELO_FLAG 3
uint contagem = INTERVALO;

bool modo_noturno = false;
TaskHandle_t xHandleCronometro = NULL;

// Headers de função
void alterarLedRgb(uint8_t cor, bool status);
void proxSinal();

void vLedRGBTask()
{
    // Inicializar LEDs
    gpio_init(led_red_pin);
    gpio_set_dir(led_red_pin, GPIO_OUT);
    gpio_put(led_red_pin, 0);

    gpio_init(led_green_pin);
    gpio_set_dir(led_green_pin, GPIO_OUT);
    gpio_put(led_green_pin, 0);

    while (true)
    {   
        if (!modo_noturno)
        {
            if (sinal_anterior != sinal_atual)
            {
                alterarLedRgb(sinal_anterior, false);
                sinal_anterior = sinal_atual;
            }

            alterarLedRgb(sinal_atual, true);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            alterarLedRgb(sinal_noturno, true);
            vTaskDelay(pdMS_TO_TICKS(1000));
            alterarLedRgb(sinal_noturno, false);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

}

void vDisplayTask()
{   
    // Configura o display
    ssd1306_t ssd;
    ssd1306_i2c_init(&ssd);

    // Buffers para armazenar string
    char str_x[9];
    char str_y[5];
    while (true)
    {   
        if (!modo_noturno)
        {
            sprintf(str_y, "%d", contagem); // Converte em string

            // Converte sinal atual em string
            switch (sinal_atual)
            {
                case VERDE:
                    sprintf(str_x, "PASSE!");
                    break;
                case AMARELO:
                    sprintf(str_x, "ATENCAO!");
                    break;
                case VERMELHO:
                    sprintf(str_x, "PARE!");
                    break;
            }

            ssd1306_fill(&ssd, false);                          // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);      // Desenha um retângulo
            ssd1306_line(&ssd, 3, 25, 123, 25, true);           // Desenha uma linha
            ssd1306_line(&ssd, 3, 37, 123, 37, true);           // Desenha uma linha
            ssd1306_draw_string(&ssd, "SEMAFORO RTOS", 13, 12);  // Desenha uma string
            ssd1306_draw_string(&ssd, str_x, 37, 28); // Desenha uma string
            ssd1306_draw_string(&ssd, "Contagem Reg.", 10, 41);    // Desenha uma string
            ssd1306_draw_string(&ssd, str_y, 55, 52);          // Desenha uma string
            ssd1306_send_data(&ssd);                           // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(735));
        }
        else
        {   
            ssd1306_fill(&ssd, false);   
            ssd1306_draw_string(&ssd, "MODO NOTURNO", 10, 28); // Desenha uma string
            ssd1306_draw_string(&ssd, "Zzz. . .", 10, 38); // Desenha uma string
            ssd1306_send_data(&ssd); 
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void vMatrizTask()
{   
    // Inicializa e limpa a matriz de LEDs
    inicializarMatriz();

    Rgb frameVerde[MATRIZ_ROWS][MATRIZ_COLS] = 
    {
        {{0, 0, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 1, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 1, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 1, 0}, {0, 0, 0}, {0, 1, 0}, {0, 0, 0}}
    };

    Rgb frameAmarelo[MATRIZ_ROWS][MATRIZ_COLS] = 
    {
        {{1, 1, 0}, {0, 0, 0}, {1, 1, 0}, {0, 0, 0}, {1, 1, 0}},
        {{1, 1, 0}, {0, 0, 0}, {1, 1, 0}, {0, 0, 0}, {1, 1, 0}},
        {{1, 1, 0}, {0, 0, 0}, {1, 1, 0}, {0, 0, 0}, {1, 1, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{1, 1, 0}, {0, 0, 0}, {1, 1, 0}, {0, 0, 0}, {1, 1, 0}}
    };

    Rgb frameVermelho[MATRIZ_ROWS][MATRIZ_COLS] = 
    {
        {{0, 0, 0}, {1, 0, 0}, {0, 0, 0}, {1, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {1, 0, 0}, {0, 0, 0}, {1, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}},
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}}
    };

    Rgb frameNoturno[MATRIZ_ROWS][MATRIZ_COLS] = 
    {
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{1, 1, 0}, {0, 0, 0}, {1, 1, 0}, {0, 0, 0}, {1, 1, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
    };


    while (true)
    {   
        if (!modo_noturno)
        {
            switch (sinal_atual)
            {
                case VERDE:
                    desenharFrame(frameVerde);
                    break;
                case AMARELO:
                    desenharFrame(frameAmarelo);
                    break;
                case VERMELHO:
                    desenharFrame(frameVermelho);
                    break;
            }
            atualizarMatriz();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            desenharFrame(frameNoturno);
            atualizarMatriz();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void vBuzzerTask()
{   
    // Parâmetros para PWM do buzzer
    uint16_t wrap = 1000;
    float div = 250.0;

    // Obter slice e definir pino como PWM
    gpio_set_function(buzzer_pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(buzzer_pin);

    // Configurar frequência
    pwm_set_wrap(slice, wrap);
    pwm_set_clkdiv(slice, div); 
    pwm_set_gpio_level(buzzer_pin, 0);
    pwm_set_enabled(slice, true);

    uint tempo_ligado;
    uint tempo_desligado;
    while (true)
    {   
        if (!modo_noturno)
        {
            switch (sinal_atual)
            {
                case VERDE:
                    tempo_ligado = 700;
                    tempo_desligado = 300;
                    break;
                case AMARELO:
                    tempo_ligado = 100;
                    tempo_desligado = 100;
                    break;
                case VERMELHO:
                    tempo_ligado = 500;
                    tempo_desligado = 1500;
            }

            pwm_set_gpio_level(buzzer_pin, 500);
            vTaskDelay(pdMS_TO_TICKS(tempo_ligado));
            pwm_set_gpio_level(buzzer_pin, 0);
            vTaskDelay(pdMS_TO_TICKS(tempo_desligado));
        }
        else
        {
            pwm_set_gpio_level(buzzer_pin, 500);
            vTaskDelay(pdMS_TO_TICKS(1200));
            pwm_set_gpio_level(buzzer_pin, 0);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

void vCronometroTask()
{
    while (true)
    {   
        while (contagem > 0)
        {   
            vTaskDelay(pdMS_TO_TICKS(800));
            contagem--;

            if ((sinal_atual == VERDE && contagem == AMARELO_FLAG) || contagem == 0)
            {
                proxSinal();
            }
        }
        contagem = INTERVALO;
    }
    
}

void vBotaoTask()
{
    gpio_init(btn_a_pin);
    gpio_set_dir(btn_a_pin, GPIO_IN);
    gpio_pull_up(btn_a_pin);

    uint anterior = 0;
    uint atual = 0;
    while (true)
    {
        atual = to_us_since_boot(get_absolute_time());
        if (!gpio_get(btn_a_pin) && atual - anterior > 200000)
        {
            modo_noturno = !modo_noturno;
            if (modo_noturno)
            {
                vTaskSuspend(xHandleCronometro);
            }
            else
            {
                sinal_atual = VERDE;
                sinal_anterior = sinal_noturno;

                contagem = INTERVALO;
                vTaskResume(xHandleCronometro);
            }
            anterior = atual;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // Fim do trecho para modo BOOTSEL com botão B

    stdio_init_all();

    xTaskCreate(vLedRGBTask, "LED RGB Task", configMINIMAL_STACK_SIZE,
         NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vMatrizTask, "Matriz LED Task", configMINIMAL_STACK_SIZE,
         NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vBuzzerTask, "Buzzer Task", configMINIMAL_STACK_SIZE,
         NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vDisplayTask, "Display Task", configMINIMAL_STACK_SIZE,
         NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vCronometroTask, "Cronometro Task", configMINIMAL_STACK_SIZE,
         NULL, tskIDLE_PRIORITY + 2, &xHandleCronometro);
    xTaskCreate(vBotaoTask, "Botão A Task", configMINIMAL_STACK_SIZE,
         NULL, tskIDLE_PRIORITY + 1, NULL);
         
    vTaskStartScheduler();
    panic_unsupported();
}

void alterarLedRgb(uint8_t cor, bool status)
{
    switch (cor)
    {
        case VERDE:
            gpio_put(led_green_pin, status);
            break;
        case AMARELO:
            gpio_put(led_green_pin, status);
            gpio_put(led_red_pin, status);
            break;
        case VERMELHO:
            gpio_put(led_red_pin, status);
            break;
        default:
            printf("COR INVÁLIDA\n");
    }
}

void proxSinal()
{   
    sinal_atual++;
    if (sinal_atual > VERMELHO)
    {
        sinal_atual = VERDE;
    }
}

// #define led1 11
// #define led2 12

// void vBlinkLed1Task()
// {
//     gpio_init(led1);
//     gpio_set_dir(led1, GPIO_OUT);
//     while (true)
//     {
//         gpio_put(led1, true);
//         vTaskDelay(pdMS_TO_TICKS(250));
//         gpio_put(led1, false);
//         vTaskDelay(pdMS_TO_TICKS(1223));
//     }
// }

// void vBlinkLed2Task()
// {
//     gpio_init(led2);
//     gpio_set_dir(led2, GPIO_OUT);
//     while (true)
//     {
//         gpio_put(led2, true);
//         vTaskDelay(pdMS_TO_TICKS(500));
//         gpio_put(led2, false);
//         vTaskDelay(pdMS_TO_TICKS(2224));
//     }
// }

// void vDisplay3Task()
// {
//     // I2C Initialisation. Using it at 400Khz.
//     i2c_init(I2C_PORT, 400 * 1000);

//     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
//     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
//     gpio_pull_up(I2C_SDA);                                        // Pull up the data line
//     gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
//     ssd1306_t ssd;                                                // Inicializa a estrutura do display
//     ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
//     ssd1306_config(&ssd);                                         // Configura o display
//     ssd1306_send_data(&ssd);                                      // Envia os dados para o display
//     // Limpa o display. O display inicia com todos os pixels apagados.
//     ssd1306_fill(&ssd, false);
//     ssd1306_send_data(&ssd);

//     char str_y[5]; // Buffer para armazenar a string
//     int contador = 0;
//     bool cor = true;
//     while (true)
//     {
//         sprintf(str_y, "%d", contador); // Converte em string
//         contador++;                     // Incrementa o contador
//         ssd1306_fill(&ssd, !cor);                          // Limpa o display
//         ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      // Desenha um retângulo
//         ssd1306_line(&ssd, 3, 25, 123, 25, cor);           // Desenha uma linha
//         ssd1306_line(&ssd, 3, 37, 123, 37, cor);           // Desenha uma linha
//         ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6); // Desenha uma string
//         ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);  // Desenha uma string
//         ssd1306_draw_string(&ssd, "  FreeRTOS", 10, 28); // Desenha uma string
//         ssd1306_draw_string(&ssd, "Contador  LEDs", 10, 41);    // Desenha uma string
//         ssd1306_draw_string(&ssd, str_y, 40, 52);          // Desenha uma string
//         ssd1306_send_data(&ssd);                           // Atualiza o display
//         sleep_ms(735);
//     }
// }
