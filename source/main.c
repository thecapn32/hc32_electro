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
#include "uart.h"
#include "flash.h"

#include "app_pindef.h"
#include "app_gpio.h"
#include "app_timer.h"
#include "app_adcdac.h"
#include "app_uart.h"

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
//wave 0 = long wave
//wave 1 = short wave
volatile int wave = 0;

/* for making buzzer beep */
volatile int beep = 0;
/* for enabling and disabling beep sound */
volatile int buzz_en = 1;

const uint32_t l_duration[18] = {30, 5, 30, 5, 240, 2, 180, 3, 180, 5, 120, 5, 360, 360, 90, 5, 90, 90};

const uint32_t s_duration[18] = {30, 5, 30, 5, 120, 2,  90, 3,  90, 5,  60, 5, 180, 180, 30, 5, 30, 30};

//testing	
//const uint16_t freq[18] = {781, 9700, 9700, 9700, 9700, 9700, 390, 0, 781, 0, 0xffff, 0, 781, 9700, 26000, 0, 78, 1562};
//                       {600, , 500, , 300, 0,  20, 0,  10, 0,    0.1, ,  10,  0.8,  0.3,  ,   100, 5
const uint16_t freq[18] = {13, 0, 16, 0, 26, 0, 390, 0, 781, 0, 0xffff, 0, 781, 9700, 26000, 0, 78, 1562};
//                       {600, , 500, , 300, 0,  20, 0,  10, 0,    0.1, ,  10,  0.8,  0.3,  ,   100, 5
const uint32_t dacCur_r[18] = {20, 0, 20, 0, 20, 0, 100, 0, 20, 0, 80, 0, 50, 80, 50, 0, 50, 100};

const uint16_t dac_16Val_pos[18] = {2203, 1975, 2203, 1975, 2203, 1975, 3015, 1975, 2203, 
                                    1975, 2796, 1975, 2532, 2796, 2532, 1975, 2532, 3015};
//have an array for first time
const int dacCur[9] = {-100, -80, -50, -20, 0, 20, 50, 80, 100};

uint16_t dacCal[9] = {752, 958, 1288, 1597, 1816, 2081, 4090, 4090, 4090};

const uint16_t test_cur[4] = {1975, 2532, 3015, 4090};

volatile uint32_t VBAT;
volatile uint32_t V_SEN;
volatile uint32_t T_SEN;
volatile uint32_t I_SEN;
/*t*/
/* for buzzer */

volatile int onOff_count = 0;

/* sw1 pressed flag for timer0 */
volatile int onOff_interrupt = 0;

/* in pause state timer counter */
volatile uint32_t pause_cnt = 0;

/* in test state timer counter */
volatile uint32_t test_cnt = 0;

/* timer0 fire flag each 10ms */
volatile int timer0_callback = 0;

uint8_t u8TxData[8] = {'1','2','3','4','5','6','7','8'};

static int test_cur_index = 0;

const uint32_t flash_Addr = 0x1ff00;

static int check_before_run(void)
{
  float v = VBAT * (4.15 / 4095.0);
  float vt = T_SEN * (3.3 / 4095.0);
  float vs = V_SEN * (3.3 / 4095.0);
  if(v < 3.4)
    return 0;
  return 1;
}

void flash_init(void)
{
   while(Ok != Flash_Init(1, TRUE))
  {
    ;
  }
}

void read_sn(uint8_t *t)
{
  for (int i = 0; i < 8; i++)
  {
    t[i] = *((volatile uint8_t*)flash_Addr + i);
  }
}

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
  onOff_count++;
  // if passed 3sec
  if (onOff_count == 300) // period is 10ms
  {
    // disable timer interrupt function
    onOff_interrupt = 0;
    // take action acording to state
    long_press_action();
  }
  // if pressed 1 sec check if sw1 is also pressed then changed
  else if (state != SLEEP && onOff_count == 100 && (FALSE == Gpio_GetInputIO(wavSelPort, wavSelPin)))
  {
    // disable timer interrupt function
    onOff_interrupt = 0;
    buzz_en = !buzz_en;
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

/* make buzzer sound t ms */
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

/* uart configuration */
void App_UartCfg(void)
{
  stc_uart_cfg_t  stcCfg;
  stc_uart_multimode_t stcMulti;
  stc_uart_baud_t stcBaud;

  DDL_ZERO_STRUCT(stcCfg);
  DDL_ZERO_STRUCT(stcMulti);
  DDL_ZERO_STRUCT(stcBaud);
  
  Sysctrl_SetPeripheralGate(SysctrlPeripheralUart0,TRUE);//UART0外设模块时钟使能
  
  stcCfg.enRunMode = UartMskMode3;     //模式3
  stcCfg.enStopBit = UartMsk1bit;      //1位停止位
  stcCfg.enMmdorCk = UartMskEven;      //偶校验
  stcCfg.stcBaud.u32Baud = 9600;       //波特率9600
  stcCfg.stcBaud.enClkDiv = UartMsk8Or16Div;         //通道采样分频配置
  stcCfg.stcBaud.u32Pclk = Sysctrl_GetPClkFreq();    //获得外设时钟（PCLK）频率值
  Uart_Init(M0P_UART0, &stcCfg);       //串口初始化

  Uart_ClrStatus(M0P_UART0,UartRC);    //清接收请求
  Uart_ClrStatus(M0P_UART0,UartTC);    //清发送请求
  Uart_EnableIrq(M0P_UART0,UartRxIrq); //使能串口接收中断
  Uart_EnableIrq(M0P_UART0,UartTxIrq); //使能串口发送中断

}

/* checking signal & change system state */
static void check_state_signal(void)
{
  if (wake)
  {
    /* clear flag */
    wake = 0;
    if(wave == 0)
    {
      Gpio_SetIO(wav0LedPort, wav0LedPin);
      Gpio_ClrIO(wav1LedPort, wav1LedPin);
      
    }
    else
    {
      Gpio_ClrIO(wav0LedPort, wav0LedPin);
      Gpio_SetIO(wav1LedPort, wav1LedPin);
    }
    
    if(state == SLEEP)
    {
      /* set gpio pin modes enable necessary pins */
      setActvGpio();
      buzz_beep(1000);
      // App_DacCali();
      Gpio_EnableIrq(wavSelPort, wavSelPin, GpioIrqFalling);
    } 
    /* it was aborted */
    else if(state == RUNNING) 
    {
      Bt_M0_Stop(TIM1);
      buzz_beep(300);
      delay1ms(500);
      buzz_beep(300);
    }
    state = WAKEUP;
    
  }
  if (run)
  {
    /* clear flag */
    run = 0;
    if(state == WAKEUP)
    {
      //check VBAT and VSEN and if okay start waves
      App_AdcJqrCfg();
      delay1ms(100);
      if(1)//check_before_run())
      {
        phase_index = 0;
        phase_cnt = 0;
        phase_cnt_target = (wave == 0) ? l_duration[0] : s_duration[0];
        /* set logic0 & logic 1 initial values */
        logic0 = dac_16Val_pos[1];
        logic1 = dac_16Val_pos[0];
        /* get the period value of timer1 for first phase */
        uint16_t u16Period = freq[0];
        /* Run timer1 for generate wave values on DAC */
        Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
        /* must be set in every phase */
        Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
        //App_AdcSglCfg();
        //Bt_M0_Run(TIM0);
        buzz_beep(300);
        delay1ms(500);
        buzz_beep(300);
        Bt_M0_Run(TIM1);
      }
      else
      {
        Gpio_SetIO(lowChrgLedPort, lowChrgLedPin);
        delay1ms(5000);
        Gpio_ClrIO(lowChrgLedPort, lowChrgLedPin);
        sleep = 1;
      }
    }
    else if(state == PAUSE) 
    {
      uint16_t u16Period = freq[phase_index];
      /* Run timer1 for generate wave values on DAC */
      Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
      /* must be set in every phase */
      Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
      //App_AdcSglCfg();
      buzz_beep(300);
      delay1ms(500);
      buzz_beep(300);
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
    Dac_SetChannelData(DacRightAlign, DacBit12, logic0);
    Dac_SoftwareTriggerCmd();
    /* Turn of DC/DC */
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
    
    Bt_M0_Stop(TIM1);
    Gpio_DisableIrq(wavSelPort, wavSelPin, GpioIrqFalling);
    /* disable dac everything else running */
    /* change device state */
    buzz_beep(1000);
    Bt_M0_Stop(TIM0);
    state = SLEEP;
    lowPowerGpios();
    Lpm_GotoDeepSleep(FALSE);
  }
  if (change_wave)
  {
    change_wave = 0;
    buzz_beep(300);
    if (wave == 0)
    {
      wave = 1;
      Gpio_ClrIO(wav0LedPort, wav0LedPin);
      Gpio_SetIO(wav1LedPort, wav1LedPin);
    }
    else
    {
      wave = 0;
      Gpio_SetIO(wav0LedPort, wav0LedPin);
      Gpio_ClrIO(wav1LedPort, wav1LedPin);
    }
  }
  if (test_mode)
  {
    test_mode = 0;
    stc_gpio_cfg_t stcGpioCfg;
    
    DDL_ZERO_STRUCT(stcGpioCfg);
    
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio,TRUE);
    stcGpioCfg.enDir = GpioDirOut;
    Gpio_Init(txPort,txPin,&stcGpioCfg);
    Gpio_SetAfMode(txPort,txPin,GpioAf2);
    stcGpioCfg.enDir = GpioDirIn;
    Gpio_Init(rxPort,rxPin,&stcGpioCfg);
    Gpio_SetAfMode(rxPort,rxPin,GpioAf2);
    // enable uart and start listening
    state = TEST;
    Bt_M0_Run(TIM0); // running timer 0 for test
    test_cur_index = 0;
  }
}

/* checking running wave phase */
static void check_phase()
{
  /* this counts number of times wave could run */
  phase_cnt++;
  /* check if it is passed half of phase duration for now disable can't produce negative current*/
  // if (phase_cnt >= phase_cnt_target * 50)
  // {
  //   /* dac value should be changed to positive current */
  //   if (freq[phase_index] != 0)
  //   {
  //     logic1 = dac_16Val_pos[phase_index];
  //   }
  // }
  if (phase_cnt == phase_cnt_target * 100)
  {
    /* Here should be where the values must be changed for running next phase */
    /* change dac values here */
    phase_index++;
    /* if any phase remains */
    if (phase_index < 18)
    {
      phase_cnt = 0;
      phase_cnt_target = (wave == 0) ? l_duration[phase_index] : s_duration[phase_index];
      /* if phase frequency is 0 (pause phase) */
      if (freq[phase_index] == 0)
      {
				Bt_M0_Stop(TIM1);
        Dac_SetChannelData(DacRightAlign, DacBit12, logic0);
        Dac_SoftwareTriggerCmd();
      }
      else
      {
        logic1 = dac_16Val_pos[phase_index];
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

  /* System configuration */
  flash_init();
  setLpGpio();
  //setActvGpio();
  App_DACInit();
  App_AdcInit_scan();
  App_UartCfg();
  App_Timer0Cfg();
  App_Timer1Cfg();
  
	//App_DacCali();
	//App_AdcSglCfg();

  /* putting system to deepsleep */
  lowPowerGpios();
  Lpm_GotoDeepSleep(FALSE);
  /* for testing */
  //run = 1;
  //Gpio_EnableIrq(wavSelPort, wavSelPin, GpioIrqFalling);

  //App_AdcInit();
  while (1)
  {
    check_state_signal();
    /* check if timer0(10ms period) is fired */
    if (timer0_callback)
    {
      timer0_callback = 0;
      /* for on_off button state */
      if (onOff_interrupt)
      {
        check_onoff();
      }
      /* if device is in running state */
      if (state == RUNNING)
      {
        check_phase();
        /* flash selected wave led */
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
      }
      /* if device is in pause state */
      if (state == PAUSE)
      {
        /* timer counter to detect if it is passed 15 minutes in pause mode */
        pause_cnt++;
        /* */
        wave_flash_cnt++;
        if (wave_flash_cnt >= 70)
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
          buzz_beep(1000);
          sleep = 1;
        }
      }
      /* if device is in test state */
      if (state == TEST)
      {
        u8TxData[0] = uart_read();
        /* got no data */
        if(u8TxData[0] != 0xff)
        {
          /* got smth from uart reset the test timer counter*/
          test_cnt = 0;
          if(u8TxData[0] == '1')
          {
            uart_send_test(test_cur_index);
            Dac_SetChannelData(DacRightAlign, DacBit12, test_cur[test_cur_index]);
            Dac_SoftwareTriggerCmd();
            test_cur_index++;
            if(test_cur_index > 3)
            {
              /*check if s\n is not written */
              uint8_t sn[8];
              read_sn(sn);
              uart_sn_value(sn);
              uart_sn_print();
              int i = 0;
              Uart_SendDataPoll(M0P_UART0,'0');
              Uart_SendDataPoll(M0P_UART0,'x');
              while(Ok != Flash_SectorErase(flash_Addr))
              {
                ;
              }
              read_sn(sn);
              uart_sn_value(sn);
              while (i < 8)
              {
                u8TxData[0] = uart_read();
                if(u8TxData[0] != 0xff)
                {
                  /* check boundaries */
                  if((u8TxData[0] <= '9' && u8TxData[0] >= '0') ||
                     (u8TxData[0] <= 'f' && u8TxData[0] >= 'a') ||
                     (u8TxData[0] <= 'F' && u8TxData[0] >= 'A'))
                  {
                    if(Ok == Flash_WriteByte(flash_Addr + i, u8TxData[0]))
                    {
                      Uart_SendDataPoll(M0P_UART0, u8TxData[0]);
                      i++;
                    }
                    else 
                    {
                      Uart_SendDataPoll(M0P_UART0, 'E');
                      Uart_SendDataPoll(M0P_UART0, 'R');
                      Uart_SendDataPoll(M0P_UART0, 'R');
                    }
                    
                  }
                }
              }
              Uart_SendDataPoll(M0P_UART0,'\n');
              Uart_SendDataPoll(M0P_UART0,'\r');
              read_sn(sn);
              uart_sn_value(sn);
              test_cur_index = 0;
            }
          }
        }
        /* timer to check if it is passed 30 in test mode */
        test_cnt++;
        if (test_cnt >= 3000)
        {
          // set gpios to normal goto sleep
        }
        /* make all leds blink HMI */

      }
    }
  }
}