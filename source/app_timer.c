#include "bt.h"
#include "gpio.h"
#include "dac.h"
#include "bgr.h"

#include "app_pindef.h"
#include "common.h"

/* system state, here it is readonly */
extern int state;

/* this is for buzzer if set to 1 it will beep*/
extern volatile int beep;

/* wave properties */
extern volatile uint16_t logic0;
extern volatile uint16_t logic1;

/* timer0 fire flag each 10mss */
extern volatile int timer0_callback;

/* Timer0 callback, called every 10ms */
void Tim0_IRQHandler(void)
{
    /* for toggling buzzer */
    static int i;
    if (TRUE == Bt_GetIntFlag(TIM0, BtUevIrq))
    {
        /* for checking in main loop */
        timer0_callback = 1;
        if (beep)
        {
            if (i == 0)
            {
                i++;
                Gpio_SetIO(buzzPort, buzzPin);
            }
            else
            {
                i = 0;
                Gpio_ClrIO(buzzPort, buzzPin);
            }
        }
        Bt_ClearIntFlag(TIM0, BtUevIrq);
    }
}

/* PCLK (1/4Mhz/256) , callback freq must be 100Hz */
void App_Timer0Cfg(void)
{
    uint16_t u16ArrValue;
    uint16_t u16CntValue;
    stc_bt_mode0_cfg_t stcBtBaseCfg;
    uint16_t u16Period = 157; // this is 10ms period clock 4Mhz/256
    DDL_ZERO_STRUCT(stcBtBaseCfg);

    Sysctrl_SetPeripheralGate(SysctrlPeripheralBaseTim, TRUE);

    stcBtBaseCfg.enWorkMode = BtWorkMode0;
    stcBtBaseCfg.enCT = BtTimer;
    stcBtBaseCfg.enPRS = BtPCLKDiv256;
    stcBtBaseCfg.enCntMode = Bt16bitArrMode;
    stcBtBaseCfg.bEnTog = FALSE;
    stcBtBaseCfg.bEnGate = FALSE;
    stcBtBaseCfg.enGateP = BtGatePositive;
    Bt_Mode0_Init(TIM0, &stcBtBaseCfg);

    u16ArrValue = 0x10000 - u16Period;
    Bt_M0_ARRSet(TIM0, u16ArrValue);

    u16CntValue = 0x10000 - u16Period;
    Bt_M0_Cnt16Set(TIM0, u16CntValue);

    Bt_ClearIntFlag(TIM0, BtUevIrq);
    Bt_Mode0_EnableIrq(TIM0);
    EnableNvic(TIM0_IRQn, IrqLevel3, TRUE);
}

/* Timer1 callback function */
void Tim1_IRQHandler(void)
{
    static uint8_t i;
    if (TRUE == Bt_GetIntFlag(TIM1, BtUevIrq))
    {
        if (i == 0)
        {
            i++;
            Dac_SetChannelData(DacRightAlign, DacBit12, logic1);
            Dac_SoftwareTriggerCmd();
        }
        else
        {
            i = 0;
            Dac_SetChannelData(DacRightAlign, DacBit12, logic0);
            Dac_SoftwareTriggerCmd();
        }
        Bt_ClearIntFlag(TIM1, BtUevIrq);
    }
}

/* timer for generating waves PCLK (1/4Mhz/32) */
void App_Timer1Cfg()
{
    stc_bt_mode0_cfg_t stcBtBaseCfg;

    DDL_ZERO_STRUCT(stcBtBaseCfg);

    Sysctrl_SetPeripheralGate(SysctrlPeripheralBaseTim, TRUE);

    stcBtBaseCfg.enWorkMode = BtWorkMode0;
    stcBtBaseCfg.enCT = BtTimer;
    stcBtBaseCfg.enPRS = BtPCLKDiv256; /* max freq 600Hz */
    stcBtBaseCfg.enCntMode = Bt16bitArrMode;
    stcBtBaseCfg.bEnTog = FALSE;
    stcBtBaseCfg.bEnGate = FALSE;
    stcBtBaseCfg.enGateP = BtGatePositive;
    Bt_Mode0_Init(TIM1, &stcBtBaseCfg);

    Bt_ClearIntFlag(TIM1, BtUevIrq);
    Bt_Mode0_EnableIrq(TIM1);
    EnableNvic(TIM1_IRQn, IrqLevel2, TRUE);
}