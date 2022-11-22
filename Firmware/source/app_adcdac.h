#ifndef APP_ADCDAC_H_
#define APP_ADCDAC_H_

/* init ADC module */
void App_DACInit(void);
/* DAC init for generating waves */
void App_AdcInit_scan(void);
void App_AdcInit_sgl(void);
/* DAC calibrate */
void App_DacCali(void);

/* V_SEN VBAT T_SEN adc sample */
void App_AdcJqrCfg(void);
/* ADC Threshold for I_SEN */
void App_AdcThrCfg(void);
/* single ADC measure for I_SEN calibration */
void App_AdcSglCfg(void);

/* ADC interrupt handler */
void Adc_IRQHandler(void);

#endif