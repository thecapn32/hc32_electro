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

#endif