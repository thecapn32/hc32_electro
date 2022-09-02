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
#include "adc.h"

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
  PAUSE,
  TEST
};
/* This variable holds the corresponding state of device */
volatile int state = SLEEP;

/* these variables are signals from interrupt tasks */
volatile int wake = 0;
volatile int run = 0;
volatile int pause = 0;
volatile int sleep = 0;
volatile int change_wave = 0;
volatile int test_mode = 0;

/* hold number of current phase in running state */
volatile uint32_t phase_index = 0;
/* hold the number 10ms passed since start of current phase */
volatile uint32_t phase_cnt = 0;
/* this is used by timer0 callback for controling duration of phase */
volatile uint32_t phase_cnt_target = 0;

volatile uint16_t logic0 = 0;
volatile uint16_t logic1 = 4000;

/* */

/* selected wave to run */
volatile int wave = 0;

/* for making buzzer beep */
volatile int beep = 0;
/* for enabling and disabling beep sound */
volatile int buzz_en = 1;

const uint32_t l_duration[18] = {10, 5, 10, 5, 10, 2, 180, 3, 180, 5, 120, 5, 360, 360, 90, 5, 90, 90};

const uint32_t s_duration[18] = {5, 5, 5, 5, 5, 2, 5, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

const uint16_t freq[18] = {13, 0, 14, 0, 25, 0, 390, 0, 781, 0, 0xffff, 0, 9765, 26039, 78, 0, 100, 1562};

const uint32_t dacCur_r[18] = {20, 0, 20, 0, 20, 0, 100, 0, 20, 0, 80, 0, 50, 80, 50, 0, 50, 100};

const int dacCur[9] = {-100, -80, -50, -20, 0, 20, 50, 80, 100};

uint16_t dacCal[9] = {752, 958, 1288, 1597, 1816, 2081, 4090, 4090, 4090};

volatile uint32_t VBAT;
volatile uint32_t V_SEN;
volatile uint32_t T_SEN;
volatile uint32_t I_SEN;
/*t*/
/* for buzzer */

volatile int press_count = 0;
/* sw1 pressed flag for timer0 */
volatile int onOff_interrupt = 0;

/* in pause state count */
volatile uint32_t pause_cnt = 0;

volatile uint32_t test_cnt = 0;

/* timer0 fire flag each 10ms */
volatile int timer0_callback = 0;

/* for signaling writing logic0,1 to DAC */
volatile int w_logic0 = 0;
volatile int w_logic1 = 0;

volatile int adc_logic;

/* Configure system clock*/
void App_ClkCfg(void)
{
  Sysctrl_ClkSourceEnable(SysctrlClkRCL, TRUE);
  Sysctrl_SysClkSwitch(SysctrlClkRCL);
  Sysctrl_SetRCHTrim(SysctrlRchFreq4MHz);
  Sysctrl_SysClkSwitch(SysctrlClkRCH);
  Sysctrl_ClkSourceEnable(SysctrlClkRCL, FALSE);
}

/* take long press action according to device state */
static void long_press_action()
{
  switch (state)
  {
  case WAKEUP:
    sleep = 1;
    break;
  case RUNNING:
    wake = 1;
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

static void buzz_beep(int t)
{
  if (buzz_en)
  {
    beep = 1;
    delay1ms(t);
    beep = 0;
    delay1ms(1);
    Gpio_ClrIO(buzzPort, buzzPin);
  }
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

static void check_state_signal(void)
{
  if (wake)
  {
    /* clear flag */
    wake = 0;
    if(wave)
    {
      Gpio_ClrIO(wav0LedPort, wav0LedPin);
      Gpio_SetIO(wav1LedPort, wav1LedPin);
    }
    else
    {
      Gpio_SetIO(wav0LedPort, wav0LedPin);
      Gpio_ClrIO(wav1LedPort, wav1LedPin);
    }
    
    if(state == SLEEP)
    {
      /* set gpio pin modes enable necessary pins */
      setActvGpio();
      // App_DacCali();
      Gpio_ClrIO(buzzPort, buzzPin);
      Gpio_EnableIrq(wavSelPort, wavSelPin, GpioIrqFalling);
    }
    state = WAKEUP;
    buzz_beep(1000);
  }
  if (run)
  {
    /* clear flag */
    run = 0;
    if(state == WAKEUP)
    {
      //check VBAT and VSEN and if okay start waves
      phase_index = 0;
      phase_cnt = 0;
      phase_cnt_target = (wave) ? l_duration[0] : s_duration[0];
      /* set logic0 & logic 1 initial values */
      logic0 = dacCal[4];
      logic1 = dacCal[3];
      /* get the period value of timer1 for first phase */
      uint16_t u16Period = freq[0];
      /* Run timer1 for generate wave values on DAC */
      Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
      /* must be set in every phase */
      Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
      App_AdcSglCfg();
      Bt_M0_Run(TIM1);
    }
    else if(state == PAUSE) 
    {
      uint16_t u16Period = freq[phase_index];
      /* Run timer1 for generate wave values on DAC */
      Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
      /* must be set in every phase */
      Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
      //App_AdcSglCfg();
      Bt_M0_Run(TIM1);
    }
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
    buzz_beep(300);
    delay1ms(500);
    buzz_beep(300);
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
    lowPowerGpios();
    Lpm_GotoDeepSleep(FALSE);
  }
  if (change_wave)
  {
    change_wave = 0;
    buzz_beep(300);
    if (wave)
    {
      wave = 0;
      Gpio_SetIO(wav0LedPort, wav0LedPin);
      Gpio_ClrIO(wav1LedPort, wav1LedPin);
    }
    else
    {
      Gpio_ClrIO(wav0LedPort, wav0LedPin);
      Gpio_SetIO(wav1LedPort, wav1LedPin);
      wave = 1;
    }
  }
  if (test_mode)
  {
    test_mode = 0;
    // enable uart and start listening
  }
}

static void check_phase()
{
  /* this counts number of times wave could run */
  phase_cnt++;
  /* check if it is passed half of phase duration */
  if (phase_cnt >= phase_cnt_target * 50)
  {
    /* dac value should be changed to positive current */
    if (freq[phase_index] != 0)
    {
      logic1 = dacCal[8 - get_dacVal_index()];
    }
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
        logic1 = logic0;
        //here also stop timer1 and only write to dac
      }
      else
      {
        logic1 = dacCal[get_dacVal_index()];
        /* change timer frequency */
        Bt_M0_Stop(TIM1);
        uint16_t u16Period = freq[phase_index];
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
      //make system go backto wakeup state
      //spot timer1
      wake = 1;
      Bt_M0_Stop(TIM1);
    }
  }
}

int32_t main(void)
{
  /* init gpios that are active in deep sleep mode */
  App_ClkCfg();
  
  int wave_flash = 0;
  int wave_flash_cnt = 0;
  /* DAC unit init */
  // App_DACInit();
  /* ADC unit init */

  /* Timer0 init */
  delay1ms(1000);
  setLpGpio();
  setActvGpio();
  App_DACInit();
  App_AdcInit();
	App_AdcSglCfg();
	App_DacCali();
  App_Timer0Cfg();
  App_Timer1Cfg();

  /* enable interrupt on on_off button */
  /* enable interrupt on chrg */
  /* enable interrupt on usb_detect */
  //lowPowerGpios();
  //Lpm_GotoDeepSleep(FALSE);
  /* */
  // run = 1;

  // App_AdcInit();
  while (1)
  {
    check_state_signal();
    //check if timer0(10ms period) is fired
    if (timer0_callback)
    {
      timer0_callback = 0;
      /* for on_off button */
      if (onOff_interrupt)
      {
        check_onoff();
      }
      if (state == RUNNING)
      {
        check_phase();
        wave_flash_cnt++;
        if (wave_flash_cnt >= 10)
        {
          wave_flash_cnt = 0;
          if (wave)
          {
            Gpio_WriteOutputIO(wav1LedPort, wav1LedPin, wave_flash);
            wave_flash = !wave_flash;
          }
          else
          {
            Gpio_WriteOutputIO(wav0LedPort, wav0LedPin, wave_flash);
            wave_flash = !wave_flash;
          }
        }
      }
      if (state == PAUSE)
      {
        pause_cnt++;
        wave_flash_cnt++;
        if (wave_flash_cnt >= 30)
        {
          wave_flash_cnt = 0;
          if (wave)
          {
            Gpio_WriteOutputIO(wav1LedPort, wav1LedPin, wave_flash);
            wave_flash = !wave_flash;
          }
          else
          {
            Gpio_WriteOutputIO(wav0LedPort, wav0LedPin, wave_flash);
            wave_flash = !wave_flash;
          }
        }
        /* if 15 min in pause */
        if (pause_cnt >= 90000)
        {
          buzz_beep(300);
          delay1ms(500);
          buzz_beep(300);
          sleep = 1;
        }
      }
      if (state == TEST)
      {
        test_cnt++;
        if (test_cnt >= 3000)
        {
          // set gpios to normal goto sleep
        }
      }
    }
  }
}