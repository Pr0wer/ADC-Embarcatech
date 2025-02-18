#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/timer.h"

#define CENTRAL_X 2048
#define CENTRAL_Y 2048
#define JS_VARIACAO 180

const uint led_pin_red = 13;
const uint led_pin_green = 11;
const uint led_pin_blue = 12;
const uint joystick_x_pin = 27;
const uint joystick_y_pin = 26;
const uint joystick_btn_pin = 22;
const uint btn_a_pin = 5;

const uint16_t WRAP = 4000;   
const float DIV = 0.0;
static volatile bool pwmStatus = true;    
uint sliceRed;
uint sliceBlue;

static volatile uint16_t joystick_dx, joystick_dy = 0;

static volatile uint atual;
static volatile uint last_time = 0;

void inicializarBtn(uint pino);
uint inicializarPWM(uint pino);
void onBtnPress(uint gpio, uint32_t events);
uint16_t tratarVariacao(uint16_t valor, uint16_t variacao);

int main()
{   
    // Configurar PWM dos LEDs vermelho e azul
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

    // Inicializar botões e configurar rotinas de interrupção
    inicializarBtn(btn_a_pin);
    inicializarBtn(joystick_btn_pin);
    gpio_set_irq_enabled_with_callback(btn_a_pin, GPIO_IRQ_EDGE_FALL, true, &onBtnPress);
    gpio_set_irq_enabled_with_callback(joystick_btn_pin, GPIO_IRQ_EDGE_FALL, true, &onBtnPress);

    stdio_init_all();

    while (true)
    {      
        // Ler posições X e Y do joystick, tratando possíveis variações
        adc_select_input(1);
        joystick_dx = tratarVariacao(abs(adc_read() - CENTRAL_X), JS_VARIACAO);
        adc_select_input(0);
        joystick_dy = tratarVariacao(abs(adc_read() - CENTRAL_Y), JS_VARIACAO);

        pwm_set_gpio_level(led_pin_blue, joystick_dy);
        pwm_set_gpio_level(led_pin_red, joystick_dx);
        sleep_ms(10);
    }
}

void inicializarBtn(uint pino)
{
    gpio_init(pino);
    gpio_set_dir(pino, GPIO_IN);
    gpio_pull_up(pino);
}

uint inicializarPWM(uint pino)
{   
    gpio_set_function(pino, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pino);

    pwm_set_wrap(slice, WRAP);
    pwm_set_clkdiv(slice, DIV); 
    pwm_set_gpio_level(pino, 0);
    pwm_set_enabled(slice, pwmStatus); 
    
    return slice;
}

void onBtnPress(uint gpio, uint32_t events)
{   
    atual = to_us_since_boot(get_absolute_time());
    if (atual - last_time > 200000)
    {   
        last_time = atual;
        if (gpio == btn_a_pin)
        {   
            pwmStatus = !pwmStatus;
            pwm_set_enabled(sliceBlue, pwmStatus);
            pwm_set_enabled(sliceRed, pwmStatus);
        }
        else if (gpio == joystick_btn_pin)
        {
            gpio_put(led_pin_green, !gpio_get(led_pin_green));
        }
    }
}

uint16_t tratarVariacao(uint16_t valor, uint16_t variacao)
{
    if (valor < variacao)
    {
        return 0;
    }
    else
    {
        return valor;
    }
}
