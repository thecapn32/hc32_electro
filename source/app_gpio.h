#ifndef APPGPIO_H_
#define APPGPIO_H_

void lowPowerGpios(void);

void setLpGpio(void);

void setActvGpio(void);

void led_setup(void);

void blink_red(int ledNum);

void blink_white(int ledNum);

void turn_on_red(int ledNum);

void turn_off_led(int ledNum);

void turn_on_white(int ledNum);

#endif