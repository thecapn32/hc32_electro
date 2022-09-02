#include "adc.h"
#include "gpio.h"
#include "bgr.h"
#include "dac.h"

#include "app_pindef.h"
#include "common.h"

/* storing sampled ADC values */
extern volatile uint32_t VBAT;
extern volatile uint32_t V_SEN;
extern volatile uint32_t T_SEN;
extern volatile uint32_t I_SEN;

uint16_t test_val = 0;

uint16_t get_val = 0;

/* calculating current from ADC value */
volatile float current;

/* for putting system to sleep when abnormal behavior seen */
extern volatile int sleep;

/* hold number of current phase in running state */
extern volatile int phase_index;

extern const uint32_t dacCur_r[18];

extern volatile int adc_logic;

/* init ADC module */
void App_AdcInit(void)
{
    stc_adc_cfg_t              stcAdcCfg;

    DDL_ZERO_STRUCT(stcAdcCfg);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralAdcBgr, TRUE); 
    Bgr_BgrEnable();
    ///< ADC ?????
    stcAdcCfg.enAdcMode         = AdcSglMode;
    stcAdcCfg.enAdcClkDiv       = AdcMskClkDiv1;
    stcAdcCfg.enAdcSampCycleSel = AdcMskSampCycle8Clk;
    stcAdcCfg.enAdcRefVolSel    = AdcMskRefVolSelExtern1;
    stcAdcCfg.enAdcOpBuf        = AdcMskBufDisable;
    stcAdcCfg.enInRef           = AdcMskInRefDisable;
    stcAdcCfg.enAdcAlign        = AdcAlignRight;
    Adc_Init(&stcAdcCfg);
}


/* DAC init for generating waves */
void App_DACInit(void)
{
    stc_dac_cfg_t  dac_initstruct;
    
    Sysctrl_SetPeripheralGate(SysctrlPeripheralDac, TRUE);
    
    dac_initstruct.boff_t = DacBoffDisable;
    dac_initstruct.ten_t  = DacTenEnable;
    dac_initstruct.sref_t = DacVoltageExRef;
    dac_initstruct.mamp_t = DacMenp03;
    dac_initstruct.tsel_t = DacSwTriger;
    dac_initstruct.align  = DacRightAlign;
    Dac_Init(&dac_initstruct);
    Dac_Cmd(TRUE);
    /* write data */
    Dac_SetChannelData(DacRightAlign, DacBit12, 0);
    /* trigger by sw */
    Dac_SoftwareTriggerCmd();
}


/* DAC calibrate */
void App_DacCali(void)
{
    
    int i = 0;
    /* enable cali pin */
    while(1)
    {
        Dac_SetChannelData(DacRightAlign, DacBit12, test_val);
				delay1ms(10);
        test_val++;
        Dac_SoftwareTriggerCmd();
        delay1ms(100);
        Adc_SGL_Start();
        delay1ms(100);
				get_val = Dac_GetDataOutputValue();
        if((int)current + 5 >= dacCur[i] && (int)current - 5 <= dacCur[i])
        {
            delay1ms(100);
            Adc_SGL_Start();
            delay1ms(10);
            if((int)current + 5 >= dacCur[i] && (int)current - 5 <= dacCur[i])
            {
                // save the calculated value somewhere 
                dacCal[i] = test_val;
                i++;
                if(i > 8)
                    break;
            }
        }
    }
}


/* V_SEN VBAT T_SEN adc sample */
void App_AdcJqrCfg(void)
{
    stc_adc_jqr_cfg_t          stcAdcJqrCfg;
    
    DDL_ZERO_STRUCT(stcAdcJqrCfg);
    
    stcAdcJqrCfg.bJqrDmaTrig = FALSE;
    stcAdcJqrCfg.u8JqrCnt    = 3;
    Adc_JqrModeCfg(&stcAdcJqrCfg);

    Adc_CfgJqrChannel(AdcJQRCH0MUX, AdcExInputCH4); // V_BAT PA04
    Adc_CfgJqrChannel(AdcJQRCH1MUX, AdcExInputCH5); // T_SEN PA05
    Adc_CfgJqrChannel(AdcJQRCH2MUX, AdcExInputCH6); // V_SEN PA06
        
    /* enable ADC interrupt */
    Adc_EnableIrq();
    EnableNvic(ADC_DAC_IRQn, IrqLevel3, TRUE);
    
    /* start sampling from adc group*/
    Adc_JQR_Start();
}


/* ADC Threshold for I_SEN */
void App_AdcThrCfg(void)
{
    stc_adc_threshold_cfg_t    stcAdcThrCfg;
    
    Adc_CfgSglChannel(AdcExInputCH7);
    
    /* ADC I_SEN */
    stcAdcThrCfg.bAdcHtCmp     = FALSE;
    stcAdcThrCfg.bAdcLtCmp     = FALSE;
    stcAdcThrCfg.bAdcRegCmp    = TRUE;
    stcAdcThrCfg.u32AdcHighThd = 0xA00;
    stcAdcThrCfg.u32AdcLowThd  = 0x400;
    stcAdcThrCfg.enSampChSel   = AdcExInputCH7;
    Adc_ThresholdCfg(&stcAdcThrCfg);
    
    /* enable ADC interrupt */
    Adc_EnableIrq();
    EnableNvic(ADC_DAC_IRQn, IrqLevel3, TRUE);
    
    /* start ADC */
    Adc_SGL_Always_Start();

}

/* single ADC measure for */
void App_AdcSglCfg(void)
{
    /* Configuring PA07 for I_SEN */
    Adc_CfgSglChannel(AdcExInputCH7);
    Adc_EnableIrq();
    EnableNvic(ADC_DAC_IRQn, IrqLevel3, TRUE);
    //Adc_SGL_Start();
}

/* ADC interrupt handler */
void Adc_IRQHandler(void)
{    
    /* These are sampling for VBAT, T_SEN, V_SEN */
    if(TRUE == Adc_GetIrqStatus(AdcMskIrqJqr))
    {
        Adc_ClrIrqStatus(AdcMskIrqJqr);
        VBAT    = Adc_GetJqrResult(AdcJQRCH0MUX);
        T_SEN   = Adc_GetJqrResult(AdcJQRCH1MUX);
        V_SEN   = Adc_GetJqrResult(AdcJQRCH2MUX);
        Adc_JQR_Stop();
    }
    /* this threshold adc for +-30% current */
    if(TRUE == Adc_GetIrqStatus(AdcMskIrqReg))
    {
        Adc_ClrIrqStatus(AdcMskIrqReg);
        I_SEN   = Adc_GetSglResult();
        float v0 = I_SEN * (2.471 / 4095.0);
        current = ( v0 - 1.24) /24.0 * 10000.0;
        /* sleep value 2 means ADC threshold passed */
        sleep = 2;
        Adc_SGL_Always_Stop();
    }
    /* this single adc for dac calibration */
    if(TRUE == Adc_GetIrqStatus(AdcMskIrqSgl))
    {
        Adc_ClrIrqStatus(AdcMskIrqSgl);
        I_SEN = Adc_GetSglResult();
        float v0 = I_SEN * (2.471 / 4095.0);
        current = (v0 - 1.24) / 24.0 * 10000.0 * 0.91;  // define CAL_CONST as 1.0 as default, and after experiment, it will be 0.970 or 1.012 like
				if(current < 0)
					current = -current;
				if(adc_logic)
				{
					adc_logic = 0;
					if(current > 15)
						sleep = 2;
				}
				else
				{
					if(dacCur_r[phase_index] - current > 15)
						sleep = 2;
				}
        Adc_SGL_Stop();
    }
}
