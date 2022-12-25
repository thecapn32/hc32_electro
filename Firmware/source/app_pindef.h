#ifndef APP_PINDEF_H_
#define APP_PINDEF_H_

/* Digital INPUTS */
#define onOffPort  GpioPortB
#define onOffPin   GpioPin5

#define chrgPort  GpioPortB
#define chrgPin   GpioPin6

#define usbPort  GpioPortC
#define usbPin   GpioPin14

#define wavSelPort GpioPortB
#define wavSelPin  GpioPin4

#define led1RPort GpioPortA
#define led1RPin GpioPin12

#define led1GPort GpioPortA
#define led1GPin GpioPin10

#define led1BPort GpioPortA
#define led1BPin GpioPin11

#define led2RPort GpioPortA
#define led2RPin GpioPin9

#define led2BPort GpioPortA
#define led2BPin GpioPin8

#define led3GPort GpioPortB
#define led3GPin GpioPin12

#define led3BPort GpioPortB
#define led3BPin GpioPin13

#define led3RPort GpioPortB
#define led3RPin GpioPin14

#define led2GPort GpioPortB
#define led2GPin GpioPin15

/* Digital OUTPUTS */
#define buzzPort GpioPortA
#define buzzPin  GpioPin15

#define wav0LedPort GpioPortA
#define wav0LedPin  GpioPin10

#define wav1LedPort GpioPortA
#define wav1LedPin  GpioPin11

#define pwrLedPort  GpioPortB
#define pwrLedPin   GpioPin11

#define fullChrgLedPort GpioPortA
#define fullChrgLedPin GpioPin8

#define lowChrgLedPort GpioPortA
#define lowChrgLedPin GpioPin9

#define pwrEnPort GpioPortB
#define pwrEnPin  GpioPin3

#define refEnPort GpioPortF
#define refEnPin GpioPin1

#define caliEnPort GpioPortF
#define caliEnPin GpioPin0

#define senEnPort GpioPortC
#define senEnPin  GpioPin15

/* Analog INPUTS */
#define vBatPort GpioPortA
#define vBatPin  GpioPin5

#define tSenPort GpioPortA
#define tSenPin  GpioPin6

#define iSenPort GpioPortA
#define iSenPin  GpioPin7

#define vSenPort GpioPortB
#define vSenPin  GpioPin0

#define vRefPort GpioPortB
#define vRefPin  GpioPin1

/* Analog OUTPUT */
#define dacPort GpioPortA
#define dacPin  GpioPin4

/* UART pins*/
#define txPort GpioPortA
#define txPin  GpioPin14

#define rxPort GpioPortA
#define rxPin  GpioPin13


#endif
