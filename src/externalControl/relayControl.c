#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include "relayControl.h"

LOG_MODULE_REGISTER(relayControl, LOG_LEVEL_INF);

bool blockingStart = true;

#define RELAY_1_CTRL	DT_ALIAS(relay1ctrl)
static const struct gpio_dt_spec relay1Pin = GPIO_DT_SPEC_GET(RELAY_1_CTRL, gpios);

#define RELAY_2_CTRL	DT_ALIAS(relay2ctrl)
static const struct gpio_dt_spec relay2Pin = GPIO_DT_SPEC_GET(RELAY_2_CTRL, gpios);





int RelayControlInit()
{
    int err;
    int result;


    err = gpio_pin_configure_dt(&relay1Pin, GPIO_OUTPUT_INACTIVE);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure RGB LED R pin: %d", err);
        result = -1;
    }

    err = gpio_pin_configure_dt(&relay2Pin, GPIO_OUTPUT_INACTIVE);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure RGB LED G pin: %d", err);
        result = -1;
    }


    return result;
}

int RelayControlStart()
{
    blockingStart = false;
    return 0;
}

int RelayControlRelayOn(uint8_t relayNumber)
{
    int err;

    switch (relayNumber)
    {
        case 1:
            err = gpio_pin_set_dt(&relay1Pin, 1);

            if (err < 0) 
            {
                LOG_ERR("Failed to turn on relay 1 : %d", err);
            }
        break;

        case 2:
            err = gpio_pin_set_dt(&relay2Pin, 1);

            if (err < 0) 
            {
                LOG_ERR("Failed to turn on relay 2 : %d", err);
            }
        break;

        default:
        LOG_ERR("Unknown relay choosen!");
        break;
    }
}

int RelayControlRelayOff(uint8_t relayNumber)
{
    int err;

    switch (relayNumber)
    {
        case 1:
            err = gpio_pin_set_dt(&relay1Pin, 0);

            if (err < 0) 
            {
                LOG_ERR("Failed to turn off relay 1 : %d", err);
            }
        break;

        case 2:
            err = gpio_pin_set_dt(&relay2Pin, 0);

            if (err < 0) 
            {
                LOG_ERR("Failed to turn off relay 2 : %d", err);
            }
        break;

        default:
        LOG_ERR("Unknown relay choosen!");
        break;
    }
}