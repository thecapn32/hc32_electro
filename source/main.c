/*
 *      Structure of the program
 *      - initialize adc timer and dac
 *      - enable interrupts on input pins of deepsleep mode
 *      - enter deep sleep mode
 *      - from it will wait on interrupts
 *      - case on_off pressed in SLEEP state:
 *          ~ 
 *          ~
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 */
#include "bt.h"
#include "sysctrl.h"
#include "gpio.h"
#include "lpm.h"

#include "app_pindef.h"
#include "app_gpio.h"
#include "app_timer.h"
#include "app_adcdac.h"
//#include "common.h"
enum device_state_t {SLEEP, WAKEUP, RUNNING, PAUSE};
/* This variable holds the corresponding state of device */
volatile int state = SLEEP;

/* these variables are signals from interrupt tasks */
volatile int wake = 0;
volatile int run = 0;
volatile int pause = 0;
volatile int sleep = 0;
volatile int change_wave = 0;

/* hold number of current phase in running state */
volatile uint32_t phase_index;
/* hold the number 10ms passed since start of current phase */
volatile uint32_t phase_cnt;
/* this is used by timer0 callback for controling duration of phase */
volatile uint32_t phase_cnt_target;

volatile uint16_t logic0;
volatile uint16_t logic1;

/* */

/* selected wave to run */
volatile int wave = 0;

/* for making buzzer buzz */
volatile int buzz_en = 0;

const uint32_t l_duration[18] = {30, 5, 30, 5, 240, 2, 180, 3, 180, 5, 120, 5, 360, 360, 90, 5, 90, 90};

const uint32_t s_duration[18] = {30, 5, 30, 5, 120, 2, 90, 3, 90, 5, 60, 5, 180, 180, 30, 5, 30, 30};

const float freq[18] = {600, 0, 500, 0, 300, 0, 20, 0, 10, 0, 0.1, 0, 10, 0.8, 0.3, 0, 100, 5};

const uint32_t dacCur_r[18] = {20, 0, 20, 0, 20, 0, 100, 0, 20, 0, 80, 0, 50, 80, 50, 0, 50, 100};

const int dacCur[9] = {-100, -80, -50, -20, 0, 20, 50, 80, 100};

uint16_t dacCal[9] = {752, 958, 1288, 1597, 1816, 2081, 4090, 4090, 4090};

volatile uint32_t VBAT;
volatile uint32_t V_SEN;
volatile uint32_t T_SEN;
volatile uint32_t I_SEN;
/*t*/

volatile int press_count;
/* sw1 pressed flag for timer0 */
volatile int onOff_interrupt;

/* in pause state count */
volatile uint32_t pause_cnt;



static void beep(int t)
{
	buzz_en = 1;
	delay1ms(t);
	buzz_en = 0;
	Gpio_ClrIO(buzzPort, buzzPin);
}

int32_t main(void)
{
    /* init gpios that are active in deep sleep mode */
    //setLpGpio();
    /* DAC unit init */
    //App_DACInit();
    /* ADC unit init */
    // App_AdcInit();
    /* Timer0 init */

    App_Timer0Cfg();
    App_Timer1Cfg();
	
	
    /* enable interrupt on on_off button */
    /* enable interrupt on chrg */
    /* enable interrupt on usb_detect */
    //lowPowerGpios();
    //Lpm_GotoDeepSleep(FALSE);
    /* */
	  wake = 1;
    while(1)
    {
        if(wake)
        {
            /* clear flag */
            wake = 0;
            /* set gpio pin modes enable necessary pins */
            setActvGpio();
					  App_DACInit();
						App_AdcInit();
						App_AdcSglCfg();
            /* 1 long beep */
            /* change state */
            state = WAKEUP;
            /* enable ADC to measure battery voltage and temp -todo */
						
            /* here also do DAC calibration */
						
            //App_DacCali();
						buzz_en = 1;
						delay1ms(1000);
						buzz_en = 0;
					  Gpio_ClrIO(buzzPort, buzzPin);
            Gpio_EnableIrq(wavSelPort, wavSelPin, GpioIrqFalling);
        }
        if(run)
        {
            /* clear flag */
            run = 0;
            phase_index = 0;
					  phase_cnt = 0;
            phase_cnt_target = (wave)? l_duration[0]:s_duration[0];
            /* set logic0 & logic 1 initial values */
            logic0 = dacCal[4];
            logic1 = dacCal[3];
            /* get the period value of timer1 for first phase */
            uint16_t u16Period = 4000 / (freq[0] * 2);
            /* Run timer1 for generate wave values on DAC */
            Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
            /* must be set in every phase */
            Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
            Bt_M0_Run(TIM1);
            /* make the wave led flash this change acording to state in timer0 callback */
            /* change device state */
            state = RUNNING;
        }
        if(pause)
        {
            /* clear flag */
            pause = 0;
            /* stop timer1 from running */
            Bt_M0_Stop(TIM1);
            /* change device state */
            state = PAUSE;
					  /* reset pause counter */
						pause_cnt = 0;
						beep(300);
					  delay1ms(500);
						beep(300);
        }
        if(sleep)
        {
            /* clear flag */
            sleep = 0;
            /* long beep */
            Bt_M0_Stop(TIM0);
            Bt_M0_Stop(TIM1);
            Gpio_DisableIrq(wavSelPort, wavSelPin, GpioIrqFalling);
            /* disable dac everything else running */
            /* change device state */
            
            state = SLEEP;
            //lowPowerGpios();
            //Lpm_GotoDeepSleep(FALSE);
        }
				if(change_wave)
				{
					change_wave = 0;
					if(wave) 
					{
                wave = 0;
                Gpio_ClrIO(wav0LedPort, wav0LedPin);
                Gpio_SetIO(wav1LedPort, wav1LedPin);
          }
          else 
					{
               wave = 1;
							 Gpio_SetIO(wav0LedPort, wav0LedPin);
               Gpio_ClrIO(wav1LedPort, wav1LedPin);
          }
				}
		}
}