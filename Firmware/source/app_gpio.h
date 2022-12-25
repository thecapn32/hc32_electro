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

void buzzer_setup(void);

void buzz_beep(int t);

void sw1_setup(void);

void sw2_setup(void);

void check_onoff(void);

#endif