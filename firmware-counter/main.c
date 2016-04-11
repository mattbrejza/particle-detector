#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/adc.h>

#define LED_PORT GPIOA
#define LED_PIN (GPIO0 | GPIO1)

#define SPI_CLK GPIO9
#define SPI_DAT GPIO10

volatile uint32_t c1 = 0;
volatile uint32_t c2 = 0;
volatile uint32_t c3 = 0;
volatile uint32_t i1 = 0;

uint32_t buff_out[4] = {0};

volatile uint16_t beep_countdown = 0;
volatile uint16_t comm_timout = 0;
volatile uint8_t shift_count = 0;
volatile uint16_t adc_countdown = 500;

void beep(void){

	int32_t j = 30;
	int32_t i;
	while(j){
		gpio_set(GPIOA, GPIO4);
		i = 4000;
		while(i)
			i--;
		gpio_clear(GPIOA, GPIO4);
		i = 480;
		while(i)
			i--;
		j--;
	}
}

void exti4_15_isr(void)
{
	if ((EXTI_PR & (GPIO6)) != 0){
		c1++;
		gpio_set(GPIOA, GPIO1);
		beep_countdown = 70;
		//beep();
		//gpio_clear(GPIOA, GPIO0);

		EXTI_PR |= (GPIO6);
	}

	if ((EXTI_PR & (GPIO7)) != 0){
		c2++;
		gpio_set(GPIOA, GPIO0);
		beep_countdown = 30;
		//beep();
		//gpio_clear(GPIOA, GPIO1);

		EXTI_PR |= (GPIO7);
	}

	//'SPI' interrupt
	if ((EXTI_PR & (GPIO9)) != 0){

		if (comm_timout == 0){     //start of exchange
			buff_out[0] = c1;
			buff_out[1] = c2;
			buff_out[2] = c3;
			buff_out[3] = i1;
			shift_count = 0;
		}


		if (buff_out[0] & 0x8000000)
			gpio_set(GPIOA, SPI_DAT);
		else
			gpio_clear(GPIOA, SPI_DAT);

		buff_out[0] = buff_out[0] << 1;
		if (buff_out[1] & 0x8000000)
			buff_out[0] =+ 1;
		buff_out[1] = buff_out[1] << 1;
		if (buff_out[2] & 0x8000000)
			buff_out[1] =+ 1;
		buff_out[2] = buff_out[2] << 1;
		if (buff_out[3] & 0x8000000)
			buff_out[2] =+ 1;
		buff_out[3] = buff_out[3] << 1;

		shift_count++;
		if (shift_count > (32*4))
			comm_timout = 0;
		else
			comm_timout = 500;



		EXTI_PR |= (GPIO9);
	}

}

void exti0_1_isr(void)
{
	c3++;
	gpio_set(GPIOF, GPIO1);
	beep_countdown = 30;
	//beep();
	//gpio_clear(GPIOF, GPIO1);

	EXTI_PR |= (GPIO1);
}

void sys_tick_handler(void)
{

	if (beep_countdown){
		beep_countdown--;
		if (beep_countdown & 1)
			gpio_set(GPIOA, GPIO4);
		else
			gpio_clear(GPIOA, GPIO4);

		if (beep_countdown == 0){
			gpio_clear(GPIOA, GPIO0 | GPIO1 | GPIO4);
			gpio_clear(GPIOF, GPIO1);
		}
	}

	if (comm_timout)
		comm_timout--;

	if (adc_countdown == 0){
		i1 = ADC1_DR;
		adc_start_conversion_regular(ADC1);
		adc_countdown = 500;
	}
	adc_countdown--;


}

int main(void)
{
    // Set clock to 48MHz (max)
    rcc_clock_setup_in_hsi_out_8mhz();

    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_reload(7999);
	systick_interrupt_enable();
	systick_counter_enable();

    // IMPORTANT: every peripheral must be clocked before use
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOF);

    // Configure GPIO C.9 as an output
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO4);

    //configure GM inputs
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO6 | GPIO7);
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO1);
    exti_select_source(EXTI1, GPIOB);
    exti_set_trigger(EXTI1, EXTI_TRIGGER_RISING);
    exti_select_source(EXTI6, GPIOA);
    exti_set_trigger(EXTI6, EXTI_TRIGGER_RISING);
    exti_select_source(EXTI7, GPIOA);
    exti_set_trigger(EXTI7, EXTI_TRIGGER_RISING);
    exti_enable_request(EXTI1);
    exti_enable_request(EXTI6);
    exti_enable_request(EXTI7);
    nvic_enable_irq(NVIC_EXTI4_15_IRQ);
    nvic_enable_irq(NVIC_EXTI0_1_IRQ);

    //configure comms interface
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, SPI_CLK);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_DAT);
    exti_select_source(EXTI9, GPIOA);
    exti_set_trigger(EXTI9, EXTI_TRIGGER_RISING);
    exti_enable_request(EXTI9);

	//adc
	rcc_periph_clock_enable(RCC_ADC);
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO3);
	uint8_t channel[] = {3};
	adc_power_off(ADC1);
	adc_calibrate_start(ADC1);
	adc_calibrate_wait_finish(ADC1);
	//adc_set_operation_mode(ADC1, ADC_MODE_SCAN); //adc_set_operation_mode(ADC1, ADC_MODE_SCAN_INFINITE);
	adc_set_operation_mode(ADC1, ADC_MODE_SCAN);
//	adc_set_single_conversion_mode(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPTIME_239DOT5);
	adc_set_regular_sequence(ADC1, 1, channel);
	adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT);
	adc_set_single_conversion_mode(ADC1);
	adc_disable_analog_watchdog(ADC1);
	adc_power_on(ADC1);

    while(1)
    {
    };

}
