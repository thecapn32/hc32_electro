#include "gpio.h"
#include "bt.h"
#include "sysctrl.h"

#include "app_gpio.h"
#include "common.h"
#include "app_pindef.h"

extern volatile int state;

extern volatile int wave;
extern volatile int press_count;
extern volatile int onOff_interrupt;

extern volatile int buzz_en;

/* make all pins ready to enter low power mode */
void lowPowerGpios(void)
{
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);

    //Sysctrl_SetFunc(SysctrlSWDUseIOEn, TRUE);

    ///< make all pins digital output
    M0P_GPIO->PAADS = 0;
    M0P_GPIO->PBADS = 0;
    M0P_GPIO->PCADS = 0;
    M0P_GPIO->PDADS = 0;
    M0P_GPIO->PEADS = 0;
    M0P_GPIO->PFADS = 0;
    
    ///< make all pins as input
    M0P_GPIO->PADIR = 0XFFFF;
    M0P_GPIO->PBDIR = 0XFFFF;
    M0P_GPIO->PCDIR = 0XFFFF;
    M0P_GPIO->PDDIR = 0XFFFF;
    M0P_GPIO->PEDIR = 0XFFFF;
    M0P_GPIO->PFDIR = 0XFFFF;
    
    ///< enable pulldown for all pins except on_off pin and vbat
    // and also leds for charging and also full charge
    M0P_GPIO->PAPD = 0xFF9F;
    M0P_GPIO->PBPD = 0xFFDF;
    M0P_GPIO->PCPD = 0xFFFF;
    M0P_GPIO->PDPD = 0xFFFF;
    M0P_GPIO->PEPD = 0xFFFF;
    M0P_GPIO->PFPD = 0xFFFF;
    //enable pullup for vbat & t_sen pin
	  M0P_GPIO->PAPU = 0x0060;
}

/* setup GPIOs that stay active during DeepSleep */
void setLpGpio(void)
{
    stc_gpio_cfg_t stcGpioCfg;
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    /* common attributes */
    stcGpioCfg.enDir = GpioDirIn;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enDrv = GpioDrvL;
    
    /* Configuring ON_OFF pin */
    stcGpioCfg.enPu = GpioPuEnable;
    stcGpioCfg.enPd = GpioPdDisable;
    Gpio_Init(onOffPort, onOffPin, &stcGpioCfg);

    /* Configuring USB_DETECT pin */
    stcGpioCfg.enPu = GpioPuDisable;
    stcGpioCfg.enPd = GpioPdEnable;
    Gpio_Init(usbPort, usbPin, &stcGpioCfg);

    /* Configuring CHRG pin */
    Gpio_Init(chrgPort, chrgPin, &stcGpioCfg);

    /* configuring Full charge led */
    stcGpioCfg.enDir = GpioDirOut;
    Gpio_Init(fullChrgLedPort, fullChrgLedPin, &stcGpioCfg);

    /* configuring Low charge led */
    Gpio_Init(lowChrgLedPort, lowChrgLedPin, &stcGpioCfg);

}

/* setup GPIOs that function in active mode */
void setActvGpio(void)
{
    stc_gpio_cfg_t stcGpioCfg;
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);

    /* common attributes */
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enDrv = GpioDrvL;
    
    /* WAVE_SELECT pin  */
    stcGpioCfg.enDir = GpioDirIn;
    stcGpioCfg.enPu = GpioPuEnable;
    stcGpioCfg.enPd = GpioPdDisable;
    Gpio_Init(wavSelPort, wavSelPin, &stcGpioCfg);

    /* POWER_LED pin */
    stcGpioCfg.enDir = GpioDirOut;
    stcGpioCfg.enPu = GpioPuDisable;
    stcGpioCfg.enPd = GpioPdEnable;
    Gpio_Init(pwrLedPort, pwrLedPin, &stcGpioCfg);
    Gpio_SetIO(pwrLedPort, pwrLedPin);

    /* BEEP pin */
    Gpio_Init(buzzPort, buzzPin, &stcGpioCfg);
    Gpio_ClrIO(buzzPort, buzzPin);

    /* wave0 LED */
    Gpio_Init(wav0LedPort, wav0LedPin, &stcGpioCfg);
    /* wave1 LED */
    Gpio_Init(wav1LedPort, wav1LedPin, &stcGpioCfg);
    if(wave)
    {
        Gpio_ClrIO(wav0LedPort, wav0LedPin);
        Gpio_ClrIO(wav1LedPort, wav1LedPin);
    }
    else
    {
        Gpio_ClrIO(wav0LedPort, wav0LedPin);
        Gpio_ClrIO(wav1LedPort, wav1LedPin);
    }
    /* PWR_EN pin  */
    Gpio_Init(pwrEnPort, pwrEnPin, &stcGpioCfg);
    Gpio_SetIO(pwrEnPort, pwrEnPin);

    /* REF_EN pin  */
    Gpio_Init(refEnPort, refEnPin, &stcGpioCfg);
    Gpio_SetIO(refEnPort, refEnPin);

    /* CALI_EN pin  */
    Gpio_Init(caliEnPort, caliEnPin, &stcGpioCfg);
    Gpio_ClrIO(caliEnPort, caliEnPin);

    /* SEN_EN pin  */
    Gpio_Init(senEnPort, senEnPin, &stcGpioCfg);
    Gpio_SetIO(senEnPort, senEnPin);

    /* VBAT_SEN pin */
    Gpio_SetAnalogMode(vBatPort, vBatPin);
    /* T_SEN pin */
    Gpio_SetAnalogMode(tSenPort, tSenPin);
    /* I_SEN pin */
    Gpio_SetAnalogMode(iSenPort, iSenPin);
    /* V_SEN pin */
    Gpio_SetAnalogMode(vSenPort, vSenPin);
    /* VREF pin */
    Gpio_SetAnalogMode(vRefPort, vRefPin);
    /* DAC pin */
    Gpio_SetAnalogMode(dacPort, dacPin);
}

/* PortB interrupt handler */
void PortB_IRQHandler(void)
{
    /* if ON_OFF pin pressed */
    if(TRUE == Gpio_GetIrqStatus(onOffPort, onOffPin))
    {
        /* this is timer0 callback function to recognise long press */
        press_count = 0;
        /*  */
        onOff_interrupt = 1;
		
        /* start timer0 */
        if(state == SLEEP)
            Bt_M0_Run(TIM0);
        /* Clear interrupt */
        Gpio_ClearIrq(onOffPort, onOffPin);
    }
    
    if(TRUE == Gpio_GetIrqStatus(wavSelPort, wavSelPin))
    {
        if(state == WAKEUP) {        
            if(wave) {
                wave = 0;
                Gpio_SetIO(wav0LedPort, wav0LedPin);
                Gpio_ClrIO(wav1LedPort, wav1LedPin);
            }
            else {
                wave = 1;
                Gpio_ClrIO(wav0LedPort, wav0LedPin);
                Gpio_SetIO(wav1LedPort, wav1LedPin);
            }
        }
        //make this happen in the hardware
        //Bt_M0_Run(TIM0); the timer0 will be always running
        buzz_en = 1;
        delay1ms(300);
        buzz_en = 0;
        Gpio_ClrIO(buzzPort, buzzPin);
        //Bt_M0_Stop(TIM0);
        Gpio_ClearIrq(wavSelPort, wavSelPin);
    }
} 
