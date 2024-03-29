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

#include "common.h"

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
volatile int wave = QUICK_WAVE_LED;

/* for making buzzer beep */
volatile int beep = 0;
/* for enabling and disabling beep sound */
volatile int buzz_en = 1;

const uint32_t l_duration[18] = {30, 5, 30, 5, 240, 2, 180, 3, 180, 5, 120, 5, 360, 360, 90, 5, 90, 90};

const uint32_t s_duration[18] = {30, 5, 30, 5, 120, 2, 90, 3, 90, 5, 60, 5, 180, 180, 30, 5, 30, 30};

// testing
// const uint16_t freq[18] = {781, 9700, 9700, 9700, 9700, 9700, 390, 0, 781, 0, 0xffff, 0, 781, 9700, 26000, 0, 78, 1562};
//                        {600, , 500, , 300, 0,  20, 0,  10, 0,    0.1, ,  10,  0.8,  0.3,  ,   100, 5
const uint16_t freq[18] = {13, 0, 16, 0, 26, 0, 390, 0, 781, 0, 0xffff, 0, 781, 9700, 26000, 0, 78, 1562};
//                       {600, , 500, , 300, 0,  20, 0,  10, 0,    0.1, ,  10,  0.8,  0.3,  ,   100, 5
const uint32_t dacCur_r[18] = {20, 0, 20, 0, 20, 0, 100, 0, 20, 0, 80, 0, 50, 80, 50, 0, 50, 100};

const uint16_t dac_16Val_pos[18] = {2156, 1952, 2156, 1952, 2156, 1952, 2993, 1952, 2156,
                                    1952, 2783, 1952, 2465, 2783, 2465, 1952, 2465, 2993};
// have an array for first time
const int dacCur[9] = {-100, -80, -50, -20, 0, 20, 50, 80, 100};

uint16_t dacCal[9] = {752, 958, 1288, 1597, 1816, 2081, 4090, 4090, 4090};

const uint16_t test_cur[4] = {1952, 2465, 2993, 3500};

volatile uint32_t VBAT;
volatile uint32_t V_SEN;
volatile uint32_t T_SEN;
volatile uint32_t I_SEN;

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

uint8_t u8TxData[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};

static int test_cur_index = 0;

const uint32_t flash_Addr = 0x1ff00;

static int check_before_run(void)
{
  float v = VBAT * (4.15 / 4095.0);
  float vt = T_SEN * (3.3 / 4095.0);
  float vs = V_SEN * (3.3 / 4095.0);
  if (VBAT < 3300)
  {
    return 0;
  }
  if (VBAT > 4200)
  {
    int wave_flash = 0;
    while (1)
    {
      Gpio_WriteOutputIO(lowChrgLedPort, lowChrgLedPin, wave_flash);
      wave_flash = !wave_flash;
      delay1ms(500);
    }
  }
  return 1;
}

void flash_init(void)
{
  while (Ok != Flash_Init(1, TRUE))
  {
    ;
  }
}

void read_sn(uint8_t *t)
{
  for (int i = 0; i < 8; i++)
  {
    t[i] = *((volatile uint8_t *)flash_Addr + i);
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

/* uart configuration */
void App_UartCfg(void)
{
  stc_uart_cfg_t stcCfg;
  stc_uart_multimode_t stcMulti;
  stc_uart_baud_t stcBaud;

  DDL_ZERO_STRUCT(stcCfg);
  DDL_ZERO_STRUCT(stcMulti);
  DDL_ZERO_STRUCT(stcBaud);

  Sysctrl_SetPeripheralGate(SysctrlPeripheralUart0, TRUE);

  stcCfg.enRunMode = UartMskMode3;
  stcCfg.enStopBit = UartMsk1bit;
  stcCfg.enMmdorCk = UartMskEven;
  stcCfg.stcBaud.u32Baud = 9600;
  stcCfg.stcBaud.enClkDiv = UartMsk8Or16Div;
  stcCfg.stcBaud.u32Pclk = Sysctrl_GetPClkFreq();
  Uart_Init(M0P_UART0, &stcCfg);

  Uart_ClrStatus(M0P_UART0, UartRC);
  Uart_ClrStatus(M0P_UART0, UartTC);
  Uart_EnableIrq(M0P_UART0, UartRxIrq);
  Uart_EnableIrq(M0P_UART0, UartTxIrq);
}

/* checking signal & change system state */
static void check_state_signal(void)
{
  if (wake)
  {
    /* clear flag */
    wake = 0;
    if (state == SLEEP)
    {
      /* set gpio pin modes enable necessary pins */
      // setActvGpio();
      led_setup();
      buzzer_setup();
      buzz_beep(LONG_BEEP_MS);
      Gpio_EnableIrq(sw2Port, sw2Pin, GpioIrqFalling);
    }
    /* it was aborted */
    else if (state == RUNNING)
    {
      /* this timer is for wave generation */
      Bt_M0_Stop(TIM1);
      buzz_beep(SHORT_BEEP_MS);
      delay1ms(500);
      buzz_beep(SHORT_BEEP_MS);
    }
    state = WAKEUP;
  }
  if (run)
  {
    /* clear flag */
    run = 0;
    turn_on_white(wave);
    // dac init
    if (state == WAKEUP)
    {
      // check VBAT and VSEN and if okay start waves
      // App_AdcJqrCfg();
      App_DACInit();

      delay1ms(100);
      if (1) // check_before_run())
      {
        /* Calibrate current values here */
        phase_index = 0;
        phase_cnt = 0;
        // phase_cnt_target = (wave == 0) ? l_duration[0] : s_duration[0];
        phase_cnt_target = l_duration[0];
        /* set logic0 & logic 1 initial values */
        logic0 = dac_16Val_pos[1];
        logic1 = dac_16Val_pos[0];
        /* get the period value of timer1 for first phase */
        uint16_t u16Period = freq[0];
        /* Run timer1 for generate wave values on DAC */
        Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
        /* must be set in every phase */
        Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
        buzz_beep(SHORT_BEEP_MS);
        delay1ms(500);
        buzz_beep(SHORT_BEEP_MS);
        Bt_M0_Run(TIM1);
      }
      // not enough power to run the wave
      else
      {
        // Gpio_SetIO(lowChrgLedPort, lowChrgLedPin);
        delay1ms(500);
        buzz_beep(LONG_BEEP_MS);
        turn_on_red(QUICK_WAVE_LED);
        turn_on_red(STD_WAVE_LED);
        turn_on_red(DEEP_WAVE_LED);

        delay1ms(500);
        buzz_beep(SHORT_BEEP_MS);
        turn_off_led(QUICK_WAVE_LED);
        turn_off_led(STD_WAVE_LED);
        turn_off_led(DEEP_WAVE_LED);

        delay1ms(500);
        buzz_beep(LONG_BEEP_MS);
        turn_on_red(QUICK_WAVE_LED);
        turn_on_red(STD_WAVE_LED);
        turn_on_red(DEEP_WAVE_LED);
        delay1ms(500);

        buzz_beep(SHORT_BEEP_MS);
        turn_off_led(QUICK_WAVE_LED);
        turn_off_led(STD_WAVE_LED);
        turn_off_led(DEEP_WAVE_LED);
        delay1ms(500);
        // Gpio_ClrIO(lowChrgLedPort, lowChrgLedPin);
        sleep = 1;
      }
    }
    else if (state == PAUSE)
    {
      uint16_t u16Period = freq[phase_index];
      /* Run timer1 for generate wave values on DAC */
      Bt_M0_ARRSet(TIM1, 0x10000 - u16Period);
      /* must be set in every phase */
      Bt_M0_Cnt16Set(TIM1, 0x10000 - u16Period);
      buzz_beep(SHORT_BEEP_MS);
      delay1ms(500);
      buzz_beep(SHORT_BEEP_MS);
      Bt_M0_Run(TIM1);
    }
    /* make the wave led flash this change acording to state in timer0 callback */
    /* change device state */
    state = RUNNING;
    /* here the selected wave white led should be always on */
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
    buzz_beep(SHORT_BEEP_MS);
    delay1ms(500);
    buzz_beep(SHORT_BEEP_MS);
  }
  if (sleep)
  {
    /* clear flag */
    sleep = 0;
    Bt_M0_Stop(TIM1);
    Gpio_DisableIrq(sw2Port, sw2Pin, GpioIrqFalling);
    /* disable dac everything else running */
    /* change device state */
    Bt_M0_Stop(TIM0);
    state = SLEEP;
    for (int i = 0; i < 4; i++)
    {
      blink_white(QUICK_WAVE_LED);
      blink_white(STD_WAVE_LED);
      blink_white(DEEP_WAVE_LED);
      delay1ms(500);
    }
    turn_off_led(QUICK_WAVE_LED);
    turn_off_led(STD_WAVE_LED);
    turn_off_led(DEEP_WAVE_LED);
    buzz_beep(LONG_BEEP_MS);
    // lowPowerGpios();
    // Lpm_GotoDeepSleep(FALSE);
  }
  if (test_mode)
  {
    test_mode = 0;
    stc_gpio_cfg_t stcGpioCfg;
    test_cnt = 0;
    DDL_ZERO_STRUCT(stcGpioCfg);
    setActvGpio();
    Sysctrl_SetFunc(SysctrlSWDUseIOEn, TRUE);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    stcGpioCfg.enDir = GpioDirOut;
    Gpio_Init(txPort, txPin, &stcGpioCfg);
    Gpio_SetAfMode(txPort, txPin, GpioAf2);
    stcGpioCfg.enDir = GpioDirIn;
    Gpio_Init(rxPort, rxPin, &stcGpioCfg);
    Gpio_SetAfMode(rxPort, rxPin, GpioAf2);
    App_UartCfg();
    // enable uart and start listening
    state = TEST;
    test_cur_index = 0;
  }
  if (change_wave)
  {
    change_wave = 0;
    buzz_beep(SHORT_BEEP_MS);
    switch (wave)
    {
    case QUICK_WAVE_LED:
      wave = STD_WAVE_LED;
      turn_off_led(QUICK_WAVE_LED);
      break;
    case STD_WAVE_LED:
      wave = DEEP_WAVE_LED;
      turn_off_led(STD_WAVE_LED);
      break;
    case DEEP_WAVE_LED:
      wave = QUICK_WAVE_LED;
      turn_off_led(DEEP_WAVE_LED);
      break;
    default:
      wave = QUICK_WAVE_LED;
      turn_off_led(STD_WAVE_LED);
      turn_off_led(DEEP_WAVE_LED);
      break;
    }
  }
}

/* checking running wave phase */
static void check_phase()
{
  /* this counts number of times wave could run */
  phase_cnt++;
  /* check if end of duration */
  if (phase_cnt == phase_cnt_target * 50 && freq[phase_index] != 0)
  {
    Gpio_SetIO(baseOutPort, baseOutPin);
  }
  else if (phase_cnt == phase_cnt_target * 100)
  {
    /* Here should be where the values must be changed for running next phase */
    /* change dac values here */
    phase_index++;
    /* if any phase remains */
    if (phase_index < 18)
    {
      phase_cnt = 0;
      // phase_cnt_target = (wave == 0) ? l_duration[phase_index] : s_duration[phase_index];
      phase_cnt_target = l_duration[phase_index];
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
      // make system go backto wakeup state
      // spot timer1
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
  sw1_setup();
  sw2_setup(); // this must be called after board wakeup
  // setLpGpio();

  // App_DACInit();
  // App_AdcInit_scan();
  App_Timer0Cfg();
  App_Timer1Cfg();

  /* putting system to deepsleep */
  // lowPowerGpios();
  // Lpm_GotoDeepSleep(FALSE);

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
      /* if device is in wakeup mode check if there is no instruction for */
      if (state == WAKEUP)
      {
        wave_flash_cnt++;
        if (wave_flash_cnt >= 50)
        {
          wave_flash_cnt = 0;
          blink_white(wave);
        }
      }
      /* if device is in running state */
      if (state == RUNNING)
      {
        /* here it must scan adc channels */
        /* two initialatin channels for one for only sensing current */
        /* one for sensing vbat temperature */
        check_phase();
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
          buzz_beep(LONG_BEEP_MS);
          sleep = 1;
        }
      }
      /* if device is in test state */
      if (state == TEST)
      {
        // change swd pins to uart
        u8TxData[0] = uart_read();
        /* got no data */
        if (u8TxData[0] != 0xff)
        {
          /* got smth from uart reset the test timer counter*/
          test_cnt = 0;
          if (u8TxData[0] == '1')
          {
            uart_send_test(test_cur_index);
            Dac_SetChannelData(DacRightAlign, DacBit12, test_cur[test_cur_index]);
            Dac_SoftwareTriggerCmd();
            test_cur_index++;
            if (test_cur_index > 3)
            {
              /*check if s\n is not written */
              uint8_t sn[8];
              sn[0] = *((volatile uint8_t *)flash_Addr + 8);
              if (sn[0] != 0x53)
              {
                /* get s/n value from uart in hex base and store it in flash */
                read_sn(sn);
                uart_sn_value(sn);
                uart_sn_print();
                while (Ok != Flash_SectorErase(flash_Addr))
                {
                  ;
                }
                read_sn(sn);
                uart_sn_value(sn);
                Uart_SendDataPoll(M0P_UART0, '0');
                Uart_SendDataPoll(M0P_UART0, 'x');
                int i = 0;
                while (i < 8)
                {
                  u8TxData[0] = uart_read();
                  if (u8TxData[0] != 0xff)
                  {
                    /* check boundaries */
                    if ((u8TxData[0] <= '9' && u8TxData[0] >= '0') ||
                        (u8TxData[0] <= 'f' && u8TxData[0] >= 'a') ||
                        (u8TxData[0] <= 'F' && u8TxData[0] >= 'A'))
                    {
                      if (Ok == Flash_WriteByte(flash_Addr + i, u8TxData[0]))
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
                Uart_SendDataPoll(M0P_UART0, '\n');
                Uart_SendDataPoll(M0P_UART0, '\r');
                /* save s/n wrote in flash so next time don't run it again */
                Flash_WriteByte(flash_Addr + 8, 0x53);
                read_sn(sn);
                uart_sn_value(sn);
              }
              test_cur_index = 0;
            }
          }
          else if (u8TxData[0] == 'A')
          {
            Uart_SendDataPoll(M0P_UART0, u8TxData[0]);
            u8TxData[0] = 0xff;
            int i = 0;
            int timeout = 0;
            int flag = 0;
            uint8_t FW[5] = {'T', ' ', 'V', 'E', 'R'};
            uint8_t SN[4] = {'T', ' ', 'S', 'N'};
            while (1)
            {
              u8TxData[0] = uart_read();
              if (u8TxData[0] != 0xff)
              {
                Uart_SendDataPoll(M0P_UART0, u8TxData[0]);
                timeout = 0;
                if (u8TxData[0] == FW[i])
                {
                  i++;
                  if (i == 5)
                  {
                    flag = 1;
                    break;
                  }
                }
                else if (i < 4 && u8TxData[0] == SN[i])
                {
                  i++;
                  if (i == 4)
                  {
                    flag = 2;
                    break;
                  }
                }
                // wrong command
                else
                {
                  flag = 0;
                  break;
                }
              }
              delay1ms(10);
              timeout++;
              if (timeout == 300)
                break;
            }
            Uart_SendDataPoll(M0P_UART0, '\n');
            Uart_SendDataPoll(M0P_UART0, '\r');
            if (flag == 1)
            {
              // print version number
              Uart_SendDataPoll(M0P_UART0, 'V');
              Uart_SendDataPoll(M0P_UART0, '0');
              Uart_SendDataPoll(M0P_UART0, '.');
              Uart_SendDataPoll(M0P_UART0, '0');
              Uart_SendDataPoll(M0P_UART0, '\n');
              Uart_SendDataPoll(M0P_UART0, '\r');
            }
            else if (flag == 2)
            {
              // print SN value
              uint8_t sn[8];
              read_sn(sn);
              uart_sn_value(sn);
            }
          }
        }
        /* timer to check if it is passed 30 in test mode */
        wave_flash_cnt++;
        if (wave_flash_cnt >= 70)
        {
          wave_flash_cnt = 0;
          Gpio_WriteOutputIO(wav0LedPort, wav0LedPin, wave_flash);
          Gpio_WriteOutputIO(wav1LedPort, wav1LedPin, wave_flash);
          Gpio_WriteOutputIO(pwrLedPort, pwrLedPin, wave_flash);
          Gpio_WriteOutputIO(fullChrgLedPort, fullChrgLedPin, wave_flash);
          Gpio_WriteOutputIO(lowChrgLedPort, lowChrgLedPin, wave_flash);
          wave_flash = !wave_flash;
        }
        test_cnt++;
        /* check if passed 30 sec and no activity */
        if (test_cnt >= 3000)
        {
          // set gpios to normal goto sleep
          sleep = 1;
        }
        /* make all leds blink HMI */
      }
    }
  }
}