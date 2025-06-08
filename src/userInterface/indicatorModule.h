#ifndef INDICATOR_MODULE_H
#define INDICATOR_MODULE_H

//Global macros used by the .c module which needs to easily be modified by the user
#define RED_LED 0
#define GREEN_LED 1
#define BLUE_LED 2

#define ON_STATE 1 //Define the on state for the LEDs
#define OFF_STATE 0 //Define the off state for the LEDs
#define FAST_BLINK_STATE 2 //Define the fast blink state for the LEDs
#define SLOW_BLINK_STATE 3 //Define the slow blink state for the LEDs

#define DEFAULT_BLINK_FAST_INTERVAL 100 //Default fast blink interval in milliseconds
#define DEFAULT_BLINK_SLOW_INTERVAL 1000 //Default slow blink interval in milliseconds

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

//Global variables that needs to be accessed outside the modules scope
typedef struct
{
    struct gpio_dt_spec ledNumber; //Store the DTS of the LED
    uint8_t ledState; //Store the current state of the LED
    uint16_t brightness; //Store the brightness of the LED 
    //The brightness is not implemented yet, will require a PWM driver to be implemented and changes to the module
} ledStruct;

typedef struct 
{
    struct gpio_dt_spec redLedNumber;
    struct gpio_dt_spec greenLedNumber;
    struct gpio_dt_spec blueLedNumber;
} rgbLedStruct;
//This struct is not used yet, but will make sense to use once the PWM driver is implemented

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int IndicatorModuleInit();

int IndicatorModuleStart();

int IndicatorModuleLedOn(uint8_t ledNumber);

int IndicatorModuleLedBlinkFast(uint8_t ledNumber);

int IndicatorModuleLedBlinkSlow(uint8_t ledNumber);

int IndicatorModuleRGBLedColor(uint8_t ledNumber ,uint8_t rValue, uint8_t gValue, uint8_t bValue);

int IndicatorModuleLedOff(uint8_t ledNumber);

#ifdef __cplusplus
}
#endif

#endif //INDICATOR_MODULE_H
