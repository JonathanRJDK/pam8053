#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include "indicatorModule.h"

LOG_MODULE_REGISTER(indicatorModule,LOG_LEVEL_INF);

#define RGB_LED_R	DT_ALIAS(ledrpin)
static const struct gpio_dt_spec rgbLEDR = GPIO_DT_SPEC_GET(RGB_LED_R, gpios);

#define RGB_LED_G	DT_ALIAS(ledgpin)
static const struct gpio_dt_spec rgbLEDG = GPIO_DT_SPEC_GET(RGB_LED_G, gpios);

#define RGB_LED_B	DT_ALIAS(ledbpin)
static const struct gpio_dt_spec rgbLEDB = GPIO_DT_SPEC_GET(RGB_LED_B, gpios);


static ledStruct ledR;
static ledStruct ledG;
static ledStruct ledB;


static ledStruct ledStructs[3]; // Array to hold all LED structs

static rgbLedStruct rgbLed;


struct k_timer blinkFastTimer;
struct k_timer blinkSlowTimer;

void blinkFastTimerCb(struct k_timer *timer_id)
{
    static bool ledStateFast = false;

    for (int i = 0; i < 3; i++)
    {
        if (ledStructs[i].ledState == FAST_BLINK_STATE)
        {
            gpio_pin_set_dt(&ledStructs[i].ledNumber, ledStateFast ? 1 : 0);
        }
    }
    ledStateFast = !ledStateFast;
}

void blinkSlowTimerCb(struct k_timer *timer_id)
{
    static bool ledStateSlow = false;

    for (int i = 0; i < 3; i++)
    {
        if (ledStructs[i].ledState == SLOW_BLINK_STATE)
        {
            gpio_pin_set_dt(&ledStructs[i].ledNumber, ledStateSlow ? 1 : 0);
        }
    }
    ledStateSlow = !ledStateSlow;
}




int IndicatorModuleInit()
{
    int err;
    int result;


    err = gpio_pin_configure_dt(&rgbLEDR, GPIO_OUTPUT_INACTIVE);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure RGB LED R pin: %d", err);
        result = -1;
    }

    err = gpio_pin_configure_dt(&rgbLEDG, GPIO_OUTPUT_INACTIVE);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure RGB LED G pin: %d", err);
        result = -1;
    }

    err = gpio_pin_configure_dt(&rgbLEDB, GPIO_OUTPUT_INACTIVE);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure RGB LED B pin: %d", err);
        result = -1;
    }

    // Initialize the ledStructs
    ledR.ledNumber = rgbLEDR;
    ledR.ledState = OFF_STATE;
    ledR.brightness = 0; // Default brightness

    ledG.ledNumber = rgbLEDG;
    ledG.ledState = OFF_STATE;
    ledG.brightness = 0; // Default brightness

    ledB.ledNumber = rgbLEDB;
    ledB.ledState = OFF_STATE;
    ledB.brightness = 0; // Default brightness

    ledStructs[0] = ledR;
    ledStructs[1] = ledG;
    ledStructs[2] = ledB;

    // Initialize the rgbLedStruct
    rgbLed.redLedNumber = rgbLEDR;
    rgbLed.greenLedNumber = rgbLEDG;
    rgbLed.blueLedNumber = rgbLEDB;


    k_timer_init(&blinkFastTimer, blinkFastTimerCb, NULL);
    k_timer_init(&blinkSlowTimer, blinkSlowTimerCb, NULL);

    return result;

}

int IndicatorModuleStart()
{
    k_timer_start(&blinkFastTimer, K_MSEC(DEFAULT_BLINK_FAST_INTERVAL), K_MSEC(DEFAULT_BLINK_FAST_INTERVAL)); // Fast blink interval
    k_timer_start(&blinkSlowTimer, K_MSEC(DEFAULT_BLINK_SLOW_INTERVAL), K_MSEC(DEFAULT_BLINK_SLOW_INTERVAL)); // Slow blink interval
}

int IndicatorModuleLedOn(uint8_t ledNumber)
{
    
    int err = 0;

    if (ledNumber >= ARRAY_SIZE(ledStructs)) 
    {
        LOG_ERR("Invalid LED number: %d", ledNumber);
        return -EINVAL; // Return error for invalid LED number
    }

    ledStructs[ledNumber].ledState = OFF_STATE;
    err = gpio_pin_set_dt(&ledStructs[ledNumber].ledNumber, 1);

    if (err < 0) 
    {
        LOG_ERR("Failed to turn on LED %d: %d", ledNumber, err);
    }

    return err;
}

int IndicatorModuleLedOff(uint8_t ledNumber)
{
    int err = 0;

    if (ledNumber >= ARRAY_SIZE(ledStructs)) 
    {
        LOG_ERR("Invalid LED number: %d", ledNumber);
        return -EINVAL; // Return error for invalid LED number
    }

    ledStructs[ledNumber].ledState = OFF_STATE;
    err = gpio_pin_set_dt(&ledStructs[ledNumber].ledNumber, 0);

    if (err < 0) 
    {
        LOG_ERR("Failed to turn off LED %d: %d", ledNumber, err);
    }

    return err;
}

int IndicatorModuleLedBlinkFast(uint8_t ledNumber)
{
    if (ledNumber >= ARRAY_SIZE(ledStructs)) 
    {
        LOG_ERR("Invalid LED number: %d", ledNumber);
        return -EINVAL; // Return error for invalid LED number
    }

    ledStructs[ledNumber].ledState = FAST_BLINK_STATE; // Set the state to fast blink  
    return 0; // No error to return, just set the state
}

int IndicatorModuleLedBlinkSlow(uint8_t ledNumber)
{
    if (ledNumber >= ARRAY_SIZE(ledStructs)) 
    {
        LOG_ERR("Invalid LED number: %d", ledNumber);
        return -EINVAL; // Return error for invalid LED number
    }

    ledStructs[ledNumber].ledState = SLOW_BLINK_STATE; // Set the state to slow blink
    return 0; // No error to return, just set the state
}

int IndicatorModuleRGBLedColor(uint8_t ledNumber ,uint8_t rValue, uint8_t gValue, uint8_t bValue);

