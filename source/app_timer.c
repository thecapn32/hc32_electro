#include "bt.h"
#include "gpio.h"
#include "dac.h"
#include "bgr.h"
#include "app_pindef.h"
#include "common.h"

/* here it is readonly */
extern int state;

/* variables for  */
extern volatile int press_count;
extern volatile int onOff_interrupt;
extern volatile int wave_run;

/* selected wave for */
extern volatile int wave;

/* hold number of current phase in running state */
extern volatile int phase_index;
/* hold the number 10ms passed since start of current phase*/
extern volatile int phase_cnt;
/* this is used by timer0 callback for controling duration of phase */
extern volatile uint32_t phase_cnt_target;

/* signal variables for interrupt to main thread communication */
extern volatile int wake;
extern volatile int run;
extern volatile int pause;
extern volatile int sleep;

/* this is for buzzer */
extern volatile int buzz_en;

/* wave properties */
extern volatile uint16_t logic0;
extern volatile uint16_t logic1;

/* timer0 fire flag each 10mss */
extern volatile int timer0_callback;

/* for signaling writing logic0,1 to DAC */
extern volatile int w_logic0;
extern volatile int w_logic1;


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
    if(press_count == 300) //period is 10ms 
    {
        // disable timer interrupt function
        onOff_interrupt = 0;
        // take action acording to state
        long_press_action();
    }
    //if the button is released -> single click
    else if(TRUE == Gpio_GetInputIO(onOffPort, onOffPin))
    { 
        // disable timer interrupt function
        onOff_interrupt = 0;
        // if single click detected in sleep state go back to sleep
        if(state == SLEEP)
        {
            sleep = 1;
        }
        else if(state == WAKEUP || state == PAUSE)
        {
            run = 1;
        }
        else if(state == RUNNING)
        {
            pause = 1;
        }
    }
    
}


/* Timer0 callback, called every 10ms */
void Tim0_IRQHandler(void)
{
    /* for toggling buzzer */
    static int bz;
    if(TRUE == Bt_GetIntFlag(TIM0, BtUevIrq))
    {
				timer0_callback = 1;
        Bt_ClearIntFlag(TIM0,BtUevIrq);
    }
}


/* PCLK (1/4Mhz/256) , callback freq must be 100Hz */
void App_Timer0Cfg(void)
{
    uint16_t                  u16ArrValue;
    uint16_t                  u16CntValue;
    stc_bt_mode0_cfg_t     stcBtBaseCfg;
    //uint16_t u16Period = 31250;
    uint16_t u16Period = 157; // this is 10ms period clock 4Mhz/256
    DDL_ZERO_STRUCT(stcBtBaseCfg);
    
    Sysctrl_SetPeripheralGate(SysctrlPeripheralBaseTim, TRUE);
    
    stcBtBaseCfg.enWorkMode = BtWorkMode0;
    stcBtBaseCfg.enCT       = BtTimer;
    stcBtBaseCfg.enPRS      = BtPCLKDiv256;
    stcBtBaseCfg.enCntMode  = Bt16bitArrMode;
    stcBtBaseCfg.bEnTog     = FALSE;
    stcBtBaseCfg.bEnGate    = FALSE;
    stcBtBaseCfg.enGateP    = BtGatePositive;
    Bt_Mode0_Init(TIM0, &stcBtBaseCfg);
    
    u16ArrValue = 0x10000 - u16Period;
    Bt_M0_ARRSet(TIM0, u16ArrValue);
    
    u16CntValue = 0x10000 - u16Period;
    Bt_M0_Cnt16Set(TIM0, u16CntValue);
    
    Bt_ClearIntFlag(TIM0,BtUevIrq);
    Bt_Mode0_EnableIrq(TIM0);
    EnableNvic(TIM0_IRQn, IrqLevel3, TRUE);
}


/* Timer1 callback function */
void Tim1_IRQHandler(void)
{
    static uint8_t i;
    if(TRUE == Bt_GetIntFlag(TIM1, BtUevIrq))
    {
        if(i == 0)
        {
            i = 1;
			w_logic0 = 1;
        }
        else
        {
            i = 0;
						w_logic1 = 1;
        }
        Bt_ClearIntFlag(TIM1,BtUevIrq);
    }
}


/* timer for generating waves PCLK (1/4Mhz/32) */
void App_Timer1Cfg()
{
    stc_bt_mode0_cfg_t     stcBtBaseCfg;
    
    DDL_ZERO_STRUCT(stcBtBaseCfg);
    
    Sysctrl_SetPeripheralGate(SysctrlPeripheralBaseTim, TRUE);
    
    stcBtBaseCfg.enWorkMode = BtWorkMode0;
    stcBtBaseCfg.enCT       = BtTimer;
    stcBtBaseCfg.enPRS      = BtPCLKDiv1; /* max freq 600Hz */
    stcBtBaseCfg.enCntMode  = Bt16bitArrMode;
    stcBtBaseCfg.bEnTog     = FALSE;
    stcBtBaseCfg.bEnGate    = FALSE;
    stcBtBaseCfg.enGateP    = BtGatePositive;
    Bt_Mode0_Init(TIM1, &stcBtBaseCfg);
    
    Bt_ClearIntFlag(TIM1,BtUevIrq);
    Bt_Mode0_EnableIrq(TIM1);
    EnableNvic(TIM1_IRQn, IrqLevel2, TRUE);
		
}