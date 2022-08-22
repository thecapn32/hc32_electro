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
#include "dac.h"

#include "app_pindef.h"
#include "app_gpio.h"
#include "app_timer.h"
#include "app_adcdac.h"
//#include "common.h"
enum device_state_t
{
  SLEEP,
  WAKEUP,
  RUNNING,
  PAUSE
};
/* This variable holds the corresponding state of device */
volatile int state = SLEEP;

/* these variables are signals from interrupt tasks */
volatile int wake = 0;
volatile int run = 0;
volatile int pause = 0;
volatile int sleep = 0;
volatile int change_wave = 0;

/* hold number of current phase in running state */
volatile uint32_t phase_index = 0;
/* hold the number 10ms passed since start of current phase */
volatile uint32_t phase_cnt = 0;
/* this is used by timer0 callback for controling duration of phase */
volatile uint32_t phase_cnt_target = 0;

volatile uint16_t logic0 = 0;
volatile uint16_t logic1 = 0;

/* */

/* selected wave to run */
volatile int wave = 0;

/* for making buzzer buzz */
volatile int buzz_en = 0;

const uint32_t l_duration[18] = {30, 5, 30, 5, 240, 2, 180, 3, 180, 5, 120, 5, 360, 360, 90, 5, 90, 90};

const uint32_t s_duration[18] = {30, 5, 30, 5, 120, 2, 90, 3, 90, 5, 60, 5, 180, 180, 30, 5, 30, 30};

const uint16_t freq[18] = {1666, 0, 2000, 0, 3333, 0, 50000, 0, 10, 0, 1, 0, 10, 8, 3, 0, 100, 5};

const uint32_t dacCur_r[18] = {20, 0, 20, 0, 20, 0, 100, 0, 20, 0, 80, 0, 50, 80, 50, 0, 50, 100};

const int dacCur[9] = {-100, -80, -50, -20, 0, 20, 50, 80, 100};

uint16_t dacCal[9] = {752, 958, 1288, 1597, 1816, 2081, 4090, 4090, 4090};

volatile uint32_t VBAT;
volatile uint32_t V_SEN;
volatile uint32_t T_SEN;
volatile uint32_t I_SEN;
/*t*/
/* for buzzer */
static int bz;
volatile int press_count = 0;
/* sw1 pressed flag for timer0 */
volatile int onOff_interrupt = 0;

/* in pause state count */
volatile uint32_t pause_cnt = 0;

/* timer0 fire flag each 10mss */
volatile int timer0_callback = 0;

/* for signaling writing logic0,1 to DAC */
volatile int w_logic0 = 0;
volatile int w_logic1 = 0;

/* take long press action according to device state */
static void long_press_action()
{
  switch (state)
  {
  case WAKEUP:
    sleep = 1;
    break;
  case RUNNING:
    pause = 1;
    break;
  case PAUSE:
    sleep = 1;
    break;
  case SLEEP:
    wake = 1;
    break;
  default:
    break;
  }
}

/* this function if for ON_OFF button */
static void check_onoff(void)
{
  press_count++;
  // if passed 3sec
  if (press_count == 300) // period is 10ms
  {
    // disable timer interrupt function
    onOff_interrupt = 0;
    // take action acording to state
    long_press_action();
  }
  // if the button is released -> single click
  else if (TRUE == Gpio_GetInputIO(onOffPort, onOffPin))
  {
    // disable timer interrupt function
    onOff_interrupt = 0;
    // if single click detected in sleep state go back to sleep
    if (state == SLEEP)
    {
      sleep = 1;
    }
    else if (state == WAKEUP || state == PAUSE)
    {
      run = 1;
    }
    else if (state == RUNNING)
    {
      pause = 1;
    }
  }
}

static void beep(int t)
{
  buzz_en = 1;
  delay1ms(t);
  buzz_en = 0;
  Gpio_ClrIO(buzzPort, buzzPin);
}

static int get_dacVal_index(void)
{
  switch (dacCur[phase_index])
  {
  case 100:
    return 0;
  case 80:
    return 1;
  case 50:
    return 2;
  case 20:
    return 3;
  default:
    return -1;
  }
}

int32_t main(void)
{
  /* init gpios that are active in deep sleep mode */
  // setLpGpio();
  /* DAC unit init */
  // App_DACInit();
  /* ADC unit init */
  // App_AdcInit();
  /* Timer0 init */
  delay1ms(1000);
  setActvGpio();
  App_DACInit();
  App_Timer0Cfg();
  App_Timer1Cfg();

  /* enable interrupt on on_off button */
  /* enable interrupt on chrg */
  /* enable interrupt on usb_detect */
  // lowPowerGpios();
  // Lpm_GotoDeepSleep(FALSE);
  /* */
  run = 1;

  // App_AdcInit();
  while (1)
  {
    if (timer0_callback)
    {
      timer0_callback = 0;
      /* for on_off button */
      if (onOff_interrupt)
      {
        check_onoff();
      }
      if (buzz_en)
      {
        if (bz == 0)
        {
          bz++;
          Gpio_SetIO(buzzPort, buzzPin);
        }
        else
        {
          bz = 0;
          Gpio_ClrIO(buzzPort, buzzPin);
        }
      }
      if (state == RUNNING)
      {
        /* this counts number of times wave could run */
        phase_cnt++;
        uint32_t target = (wave) ? l_duration[phase_index] : s_duration[phase_index];
        /* check if it is passed half of phase duration */
        if (phase_cnt >= phase_cnt_target * 50)
        {
          /* dac value should be changed to positive current */
          // if(freq[phase_index] != 0)
          // {
          //     logic1 = dacCal[8 - get_dacVal_index()];
          //}
        }
        if (phase_cnt == phase_cnt_target * 100)
        {
          /* Here should be where the values must be changed for running next phase */
          /* change dac values here */
          phase_index++;
          /* if any phase remains */
          if (phase_index < 18)
          {
            phase_cnt = 0;
            phase_cnt_target = (wave) ? l_duration[phase_index] : s_duration[phase_index];
            /* if phase frequency is 0 (Idle phase) */
            if (freq[phase_index] == 0)
            {
              logic1 = 1900;
              logic0 = 1900;
            }
            else
            {
              // logic1 = dacCal[get_dacVal_index()];
              logic1 = 0;
              logic0 = 4000;
              /* change timer frequency */
              Bt_M0_Stop(TIM1);
              uint16_t u16Period = freq[phase_index] * 2;
              /* Run timer1 for generate wave values on DAC */
              Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
              /* must be set in every phase */
              Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
              Bt_M0_Run(TIM1);
            }
          }
          /* it is over device must go back to sleep */
          else
          {
            sleep = 1;
          }
        }
      }
    }
    if (wake)
    {
      /* clear flag */
      wake = 0;
      /* set gpio pin modes enable necessary pins */

      App_DACInit();
      App_AdcInit();
      App_AdcSglCfg();
      /* 1 long beep */
      /* change state */
      state = WAKEUP;
      /* enable ADC to measure battery voltage and temp -todo */

      /* here also do DAC calibration */

      // App_DacCali();
      buzz_en = 1;
      delay1ms(1000);
      buzz_en = 0;
      delay1ms(5);
      Gpio_ClrIO(buzzPort, buzzPin);
      Gpio_EnableIrq(wavSelPort, wavSelPin, GpioIrqFalling);
    }
    if (run)
    {
      /* clear flag */
      run = 0;
      phase_index = 0;
      phase_cnt = 0;
      phase_cnt_target = (wave) ? l_duration[0] : s_duration[0];
      /* set logic0 & logic 1 initial values */
      logic0 = 0;
      logic1 = 4000;
      /* get the period value of timer1 for first phase */
      uint16_t u16Period = 2 * freq[0];
      /* Run timer1 for generate wave values on DAC */
      Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
      /* must be set in every phase */
      Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
      Bt_M0_Run(TIM0);
      Bt_M0_Run(TIM1);
      /* make the wave led flash this change acording to state in timer0 callback */
      /* change device state */
      state = RUNNING;
    }
    if (pause)
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
    if (sleep)
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
      // lowPowerGpios();
      // Lpm_GotoDeepSleep(FALSE);
    }
    if (change_wave)
    {
      change_wave = 0;
      if (wave)
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
    if (w_logic0)
    {
      w_logic0 = 0;
      Dac_SetChannelData(DacRightAlign, DacBit12, logic0);
      Dac_SoftwareTriggerCmd();
    }
    if (w_logic1)
    {
      w_logic1 = 0;
      Dac_SetChannelData(DacRightAlign, DacBit12, logic1);
      Dac_SoftwareTriggerCmd();
    }
  }
}