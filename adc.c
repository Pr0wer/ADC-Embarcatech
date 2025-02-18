#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "Display/ssd1306.h"

#define CENTRAL_X 2048
#define CENTRAL_Y 2048
#define JS_VARIACAO 180
#define SQUARE_WIDTH 8
#define SQUARE_HEIGHT 8
#define CENTRAL_PIXEL_X 61
#define CENTRAL_PIXEL_Y 29

// Pinos
const uint led_pin_red = 13;
const uint led_pin_green = 11;
const uint led_pin_blue = 12;
const uint joystick_x_pin = 27;
const uint joystick_y_pin = 26;
const uint joystick_btn_pin = 22;
const uint btn_a_pin = 5;

// Variáveis para PWM
const uint16_t WRAP = 10000;   
const float DIV = 0.0;
static volatile bool pwmStatus = true;    
uint sliceRed;
uint sliceBlue;

// Posição do quadrado e tamanho da borda no display
ssd1306_t ssd;
static volatile uint pixel_x = CENTRAL_PIXEL_X;
static volatile uint pixel_y = CENTRAL_PIXEL_Y;
static volatile uint pixel_x_novo;
static volatile uint pixel_y_novo;
static volatile uint8_t bordaSize = 1;

// Valores para conversão da variação do joystick para variação de pixels no display
const uint16_t jsx_por_pixel = CENTRAL_X / ((WIDTH - SQUARE_WIDTH) / 2.0);
const uint16_t jsy_por_pixel = CENTRAL_Y / ((HEIGHT - SQUARE_HEIGHT) / 2.0);

// Variação do joystick (X e Y)
static int16_t joystick_dx, joystick_dy = 0;

// Tratamento de efeito bounce
static volatile uint atual;
static volatile uint last_time = 0;

// Armazena se houve uma alteração para o display ou não 
static volatile bool novaInfo = true;

// Headers de funções
void inicializarBtn(uint pino);
uint inicializarPWM(uint pino);
void onBtnPress(uint gpio, uint32_t events);
void tratarVariacao(int16_t *valor, uint16_t variacao);
void limitarCoord(volatile uint *valor, uint min, uint max);

int main()
{   
    // Obter slice e configurar PWM dos LEDs
    sliceRed = inicializarPWM(led_pin_red);
    sliceBlue = inicializarPWM(led_pin_blue);

    // Inicializar LED verde
    gpio_init(led_pin_green);
    gpio_set_dir(led_pin_green, GPIO_OUT);
    gpio_put(led_pin_green, 0);

    // Inicializar ADC para o joystick
    adc_init();
    adc_gpio_init(joystick_x_pin);
    adc_gpio_init(joystick_y_pin);

    // Configurar display SSD1306
    ssd1306_i2c_init(&ssd);

    // Inicializar botões e configurar rotinas de interrupção
    inicializarBtn(btn_a_pin);
    inicializarBtn(joystick_btn_pin);
    gpio_set_irq_enabled_with_callback(btn_a_pin, GPIO_IRQ_EDGE_FALL, true, &onBtnPress);
    gpio_set_irq_enabled_with_callback(joystick_btn_pin, GPIO_IRQ_EDGE_FALL, true, &onBtnPress);

    stdio_init_all();

    while (true)
    {      
        // Ler variação das posições X e Y do joystick dado a posição central
        adc_select_input(1);
        joystick_dx = adc_read() - CENTRAL_X;
        adc_select_input(0);
        joystick_dy = adc_read() - CENTRAL_Y;

        // Tratar possíveis variações da posição central do joystick
        tratarVariacao(&joystick_dx, JS_VARIACAO);
        tratarVariacao(&joystick_dy, JS_VARIACAO);

        // Obter nova posição do quadrado a partir da variação do joystick
        pixel_x_novo = CENTRAL_PIXEL_X + round(joystick_dx / (float) jsx_por_pixel);
        pixel_y_novo = CENTRAL_PIXEL_Y - round(joystick_dy / (float) jsy_por_pixel);

        // Limitar valor das coordenadas entre as extremidades, considerando se existe borda 
        limitarCoord(&pixel_x_novo, 0 + bordaSize, WIDTH - SQUARE_WIDTH - bordaSize);
        limitarCoord(&pixel_y_novo, 0 + bordaSize, HEIGHT - SQUARE_HEIGHT - bordaSize);

        // Se a borda não foi alterada
        if (!novaInfo)
        {   
            // Armazena buffer com a informação do joystick ter movido ou não
            novaInfo = pixel_x != pixel_x_novo || pixel_y != pixel_y_novo;
        }   

        // Se há uma nova informação (movimentação do joystick ou nova borda)
        if (novaInfo)
        {   
            // Alterar posição atual com a nova
            pixel_x = pixel_x_novo;
            pixel_y = pixel_y_novo;

            // Atualizar posição do quadrado no display
            ssd1306_fill(&ssd, false);
            ssd1306_rect(&ssd, pixel_y, pixel_x, SQUARE_WIDTH, SQUARE_HEIGHT, 1, true, true);

            // Adicionar borda, se houver
            if (bordaSize)
            {
                ssd1306_rect(&ssd, 0, 0, WIDTH, HEIGHT, bordaSize, true, false);
            }

            ssd1306_send_data(&ssd);

            // Indica ao programa que não há mais informações novas
            novaInfo = false;
        }

        // Aumentar intensidade do respectivo LED quando a variação aumentar
        pwm_set_gpio_level(led_pin_red, abs(joystick_dx));
        pwm_set_gpio_level(led_pin_blue, abs(joystick_dy));

        // Pequeno intervalo
        sleep_ms(10);
    }
}

// Inicializa o botão em um dado pino
void inicializarBtn(uint pino)
{
    gpio_init(pino);
    gpio_set_dir(pino, GPIO_IN);
    gpio_pull_up(pino);
}

// Inicializa e configura o PWM de um LED
uint inicializarPWM(uint pino)
{   
    // Obter slice e definir pino como PWM
    gpio_set_function(pino, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pino);

    // Configurar frequência
    pwm_set_wrap(slice, WRAP);
    pwm_set_clkdiv(slice, DIV); 

    // Iniciar PWM com duty cicle 0%
    pwm_set_gpio_level(pino, 0);
    pwm_set_enabled(slice, pwmStatus); 
    
    return slice;
}

// Rotina de interrupção dos botões
void onBtnPress(uint gpio, uint32_t events)
{   
    // Tratar efeito bounce
    atual = to_us_since_boot(get_absolute_time());
    if (atual - last_time > 200000)
    {   
        last_time = atual;
        if (gpio == btn_a_pin)
        {   
            // Alternar estado do PWM
            pwmStatus = !pwmStatus;
            pwm_set_enabled(sliceBlue, pwmStatus);
            pwm_set_enabled(sliceRed, pwmStatus);
        }
        else if (gpio == joystick_btn_pin)
        {   
            // Alternar estado do LED verde
            gpio_put(led_pin_green, !gpio_get(led_pin_green));
            
            // Alterar tamanho da borda no display
            bordaSize++;
            bordaSize %= 3;

            // "Pede" ao programa para atualizar o display com a nova borda
            novaInfo = true;
        }
    }
}

// Trata uma dada variação presente em um valor
void tratarVariacao(int16_t *valor, uint16_t variacao)
{   
    // Caso o valor esteja dentro da faixa de variação
    if (abs(*valor) < variacao)
    {   
        // Considerar como alteração nula
        *valor = 0;
    }
}

// Limita um valor de uma coordenada entre as extremidades estabelecidas
void limitarCoord(volatile uint *valor, uint min, uint max)
{
    if (*valor < min)
    {
        *valor = min;
    }
    else if (*valor > max)
    {
        *valor = max;
    }
}
