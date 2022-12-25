#include "gpio.h"
#include "bt.h"
#include "sysctrl.h"

#include "app_gpio.h"
#include "common.h"
#include "app_pindef.h"

#define QUICK_WAVE_LED 1
#define STD_WAVE_LED 2
#define DEEP_WAVE_LED 3

extern volatile int state;

extern volatile int wave;
extern volatile int onOff_count;
extern volatile int onOff_interrupt;

extern volatile int buzz_en;
/* for changing the selected wave */
extern volatile int change_wave;
extern volatile int test_mode;

void turn_on_white(int ledNum)
{
    switch (ledNum)
    {
    case 1:
        Gpio_ClrIO(led1RPort, led1RPin);
        Gpio_ClrIO(led1GPort, led1GPin);
        Gpio_ClrIO(led1BPort, led1BPin);
        break;
    case 2:
        Gpio_ClrIO(led2RPort, led2RPin);
        Gpio_ClrIO(led2GPort, led2GPin);
        Gpio_ClrIO(led2BPort, led2BPin);
        break;
    case 3:
        Gpio_ClrIO(led3RPort, led3RPin);
        Gpio_ClrIO(led3GPort, led3GPin);
        Gpio_ClrIO(led3BPort, led3BPin);
        break;
    }
}

void turn_off_led(int ledNum)
{
    switch (ledNum)
    {
    case 1:
        Gpio_SetIO(led1RPort, led1RPin);
        Gpio_SetIO(led1GPort, led1GPin);
        Gpio_SetIO(led1BPort, led1BPin);
        break;
    case 2:
        Gpio_SetIO(led2RPort, led2RPin);
        Gpio_SetIO(led2GPort, led2GPin);
        Gpio_SetIO(led2BPort, led2BPin);
        break;
    case 3:
        Gpio_SetIO(led3RPort, led3RPin);
        Gpio_SetIO(led3GPort, led3GPin);
        Gpio_SetIO(led3BPort, led3BPin);
        break;
    }
}

void turn_on_red(int ledNum)
{
    switch (ledNum)
    {
    case 1:
        Gpio_ClrIO(led1RPort, led1RPin);
        Gpio_SetIO(led1GPort, led1GPin);
        Gpio_SetIO(led1BPort, led1BPin);
        break;
    case 2:
        Gpio_ClrIO(led2RPort, led2RPin);
        Gpio_SetIO(led2GPort, led2GPin);
        Gpio_SetIO(led2BPort, led2BPin);
        break;
    case 3:
        Gpio_ClrIO(led3RPort, led3RPin);
        Gpio_SetIO(led3GPort, led3GPin);
        Gpio_SetIO(led3BPort, led3BPin);
        break;
    }
}

void blink_white(int ledNum)
{
    static int i = 0;
    if (i)
    {
        turn_on_white(ledNum);
        i = 1;
    }
    else
    {
        turn_off_led(ledNum);
        i = 0;
    }
}

void blink_red(int ledNum)
{
    static int i = 0;
    if (i)
    {
        turn_on_red(ledNum);
        i = 1;
    }
    else
    {
        turn_off_led(ledNum);
        i = 0;
    }
}

void led_setup(void)
{
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    stc_gpio_cfg_t stcGpioCfg;
    stcGpioCfg.enDir = GpioDirOut;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enPu = GpioPuDisable;
    stcGpioCfg.enPd = GpioPdDisable;

    Gpio_Init(led1RPort, led1RPin, &stcGpioCfg);
    Gpio_Init(led1GPort, led1GPin, &stcGpioCfg);
    Gpio_Init(led1BPort, led1BPin, &stcGpioCfg);

    Gpio_Init(led2RPort, led2RPin, &stcGpioCfg);
    Gpio_Init(led2GPort, led2GPin, &stcGpioCfg);
    Gpio_Init(led2BPort, led2BPin, &stcGpioCfg);

    Gpio_Init(led3RPort, led3RPin, &stcGpioCfg);
    Gpio_Init(led3GPort, led3GPin, &stcGpioCfg);
    Gpio_Init(led3BPort, led3BPin, &stcGpioCfg);

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
}

/* make all pins ready to enter low power mode */
void lowPowerGpios(void)
{
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);

    ///< make all pins digital
    M0P_GPIO->PAADS = 0;
    M0P_GPIO->PBADS = 0;
    M0P_GPIO->PCADS = 0;
    M0P_GPIO->PDADS = 0;
    M0P_GPIO->PEADS = 0;
    M0P_GPIO->PFADS = 0;
    
    ///< make all pins as input
    //except GREEN_LED(PA08), RED_LED(PA09)
    M0P_GPIO->PADIR = 0XFCFF;
    M0P_GPIO->PBDIR = 0XFFFF;
    M0P_GPIO->PCDIR = 0XFFFF;
    M0P_GPIO->PDDIR = 0XFFFF;
    M0P_GPIO->PEDIR = 0XFFFF;
    M0P_GPIO->PFDIR = 0XFFFF;
    
    ///< enable pulldown for all pins except on_off pin and vbat
    // and also leds for charging and also full charge
    M0P_GPIO->PAPD = 0xFF9F;  //vbat(PA05) t_sen(PA06) is pullup
    M0P_GPIO->PBPD = 0xFFCF;  //on_off(in setLpGpio func) & wave_select(here) input is pullup it is configured
    M0P_GPIO->PCPD = 0xFFFF;
    M0P_GPIO->PDPD = 0xFFFF;
    M0P_GPIO->PEPD = 0xFFFF;
    M0P_GPIO->PFPD = 0xFFFF;
    //enable pullup for vbat & t_sen pin
	M0P_GPIO->PAPU = 0x0060;
    M0P_GPIO->PBPU = 0x0030; //enable pullup
}

/* setup GPIOs that stay active during DeepSleep */
void setLpGpio(void)
{
    stc_gpio_cfg_t stcGpioCfg;
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    
    /* Configuring ON_OFF pin */
    stcGpioCfg.enDir = GpioDirIn;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enPu = GpioPuEnable;
    stcGpioCfg.enPd = GpioPdDisable;
    Gpio_Init(onOffPort, onOffPin, &stcGpioCfg);
    EnableNvic(PORTB_IRQn, IrqLevel3, TRUE);
    Gpio_EnableIrq(onOffPort, onOffPin, GpioIrqFalling);

    /* Configuring USB_DETECT pin */
    stcGpioCfg.enDir = GpioDirIn;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enPu = GpioPuDisable;
    stcGpioCfg.enPd = GpioPdEnable;
    Gpio_Init(usbPort, usbPin, &stcGpioCfg);
    Gpio_EnableIrq(usbPort, usbPin, GpioIrqRising);
	EnableNvic(PORTC_E_IRQn, IrqLevel3, TRUE);

    /* Configuring CHRG pin */
    stcGpioCfg.enDir = GpioDirIn;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enPu = GpioPuDisable;
	  stcGpioCfg.enPd = GpioPdEnable;
    Gpio_Init(chrgPort, chrgPin, &stcGpioCfg);

    /* configuring Full charge led */
    stcGpioCfg.enDir = GpioDirOut;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enPu = GpioPuDisable;
    stcGpioCfg.enPd = GpioPdEnable;
    Gpio_Init(fullChrgLedPort, fullChrgLedPin, &stcGpioCfg);

    stcGpioCfg.enDir = GpioDirOut;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enPd = GpioPdEnable;
    stcGpioCfg.enOD = GpioOdDisable;
    /* configuring Low charge led */
    Gpio_Init(lowChrgLedPort, lowChrgLedPin, &stcGpioCfg);
}

/* setup GPIOs that function in active mode */
void setActvGpio(void)
{
    stc_gpio_cfg_t stcGpioCfg;
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
	Sysctrl_SetFunc(SysctrlSWDUseIOEn, FALSE);
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
    static int i;
    /* if sw1(ON_OFF) pin pressed */
    if(TRUE == Gpio_GetIrqStatus(onOffPort, onOffPin))
    {
        /* this is timer0 callback function to recognise long press */
        onOff_count = 0;
        /* enable flag */
        onOff_interrupt = 1;
		
        /* start timer0 */
        if(state == SLEEP) {
            Bt_M0_Run(TIM0);
        }
        /* Clear interrupt */
        Gpio_ClearIrq(onOffPort, onOffPin);
    }
    
    /* if sw2 pin pressed */
    if(TRUE == Gpio_GetIrqStatus(wavSelPort, wavSelPin))
    {
        if(state == WAKEUP) 
        {
            change_wave = 1;
        }
        Gpio_ClearIrq(wavSelPort, wavSelPin);
    }
    
    /* chrg pin HIGH -> LOW */
    if(TRUE == Gpio_GetIrqStatus(chrgPort, chrgPin))
    {
        Gpio_ClearIrq(chrgPort, chrgPin);
        if(TRUE == Gpio_GetInputIO(chrgPort, chrgPin))
        {
            Gpio_ClrIO(lowChrgLedPort, lowChrgLedPin);
            Gpio_DisableIrq(usbPort, usbPin, GpioIrqFalling);
        }
    }

}

/* PortC interrupt handler */
void PortC_IRQHandler(void)
{
    if(TRUE == Gpio_GetIrqStatus(usbPort, usbPin))
    {
        /* USB is plugged in */
        if(TRUE == Gpio_GetInputIO(usbPort, usbPin))
        {
            Gpio_DisableIrq(usbPort, usbPin, GpioIrqRising);
            Gpio_EnableIrq(usbPort, usbPin, GpioIrqFalling);
            Gpio_SetIO(lowChrgLedPort, lowChrgLedPin);
            /* if battery is charging */
            // if(TRUE == Gpio_GetInputIO(chrgPort, chrgPin))
            // {
            //     Gpio_SetIO(lowChrgLedPort, lowChrgLedPin);
            //     //Gpio_EnableIrq(chrgPort, chrgPin, GpioIrqFalling);
            // }
            // else
            // {
            //     Gpio_SetIO(fullChrgLedPort, fullChrgLedPin);
            // }
        }
        /* USB is plugged out */
        else
        {
            /* turn off leds and make it so it can detect if plugged again */
            Gpio_ClrIO(lowChrgLedPort, lowChrgLedPin);
            //Gpio_ClrIO(fullChrgLedPort, fullChrgLedPin);
            //Gpio_DisableIrq(chrgPort, chrgPin, GpioIrqFalling);
            Gpio_DisableIrq(usbPort, usbPin, GpioIrqFalling);
            Gpio_EnableIrq(usbPort, usbPin, GpioIrqRising);
        }
        Gpio_ClearIrq(usbPort, usbPin);
    }
}