#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include "buttonModule.h"

LOG_MODULE_REGISTER(buttonsHandler, LOG_LEVEL_INF);

#define BUTTON_0_NODE DT_ALIAS(sw0)
#define BUTTON_1_NODE DT_ALIAS(sw1)
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(BUTTON_0_NODE, gpios);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(BUTTON_1_NODE, gpios);

static struct gpio_callback button0CbData;
static struct gpio_callback button1CbData;

buttonsHandlerEventCb buttonEventCb;
buttonsHandlerEvent status;

void buttonPressedCb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{   
    // Check which button was pressed based on the pin number
    const struct gpio_dt_spec *buttonPressed = NULL;
    if (dev == button0.port && cb->pin_mask & BIT(button0.pin)) 
    {
        buttonPressed = &button0;
        status.buttonId = 0; // Button 0
    } 
    else if (dev == button1.port && cb->pin_mask & BIT(button1.pin)) 
    {
        buttonPressed = &button1;
        status.buttonId = 1; // Button 1
    } 
    else 
    {
        LOG_ERR("Unknown button pressed");
        return; // Unknown button, exit the callback
    }

    int value = gpio_pin_get_dt(buttonPressed); // Read the current pin state

    if(value == 0) // Button pressed (assuming active low)
    {
        LOG_INF("Button released");
        status.status = BUTTON_RELEASED;
    }
    else // Button released
    {
        LOG_INF("Button pressed");
        status.status = BUTTON_PRESSED;
    }
    (*buttonEventCb)(&status);
    
}


int ButtonsHandlerInit(buttonsHandlerEventCb eventHandler)
{
    int err;
    // Check if the event handler is NULL
    if (eventHandler == NULL) 
    {
        printk("Error: Event handler callback is NULL\n");
        return -EINVAL;
    }

    buttonEventCb = eventHandler;

    // Initialize the button GPIOs
    LOG_INF("Initializing button handler...");
    if (!device_is_ready(button0.port)) 
    {
        printk("Error: Button device not ready\n");
        return -ENODEV;
    }

    if (!device_is_ready(button1.port)) 
    {
        printk("Error: Button device not ready\n");
        return -ENODEV;
    }


    err = gpio_pin_configure_dt(&button0, GPIO_INPUT);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure button pin: %d", err);
        return err;
    }

    err = gpio_pin_configure_dt(&button1, GPIO_INPUT);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure button pin: %d", err);
        return err;
    }
    LOG_INF("Button pin configured successfully!");
    
    gpio_init_callback(&button0CbData, buttonPressedCb, BIT(button0.pin));
    gpio_add_callback(button0.port, &button0CbData);

    gpio_init_callback(&button1CbData, buttonPressedCb, BIT(button1.pin));
    gpio_add_callback(button1.port, &button1CbData);


    err = gpio_add_callback(button0.port, &button0CbData);
    if (err < 0) 
    {
        LOG_ERR("Failed to add button callback: %d", err);
        return err;
    }

    err = gpio_add_callback(button1.port, &button1CbData);
    if (err < 0) 
    {
        LOG_ERR("Failed to add button callback: %d", err);
        return err;
    }

    LOG_INF("Button callback added successfully!");
    LOG_INF("Button interrupt configured! Remeber to start the module!\n");

}

int ButtonsHandlerStart(gpio_flags_t interruptMode)
{
 //Enable the button interrupt
    LOG_INF("Starting button handler, enabling button interrupt");
    int err = gpio_pin_interrupt_configure_dt(&button0, interruptMode);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure button interrupt: %d", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&button1, interruptMode);
    if (err < 0) 
    {
        LOG_ERR("Failed to configure button 0 interrupt: %d", err);
        return err;
    }
    
    // Set the initial status
    status.status = BUTTON_INIT;

    return 0; // Success
}

int ButtonsHandlerStop(void)
{
    //Disable the button interrupt
    LOG_INF("Stopping button handler, disabling button interrupt");
    int err = gpio_pin_interrupt_configure_dt(&button0, GPIO_INT_DISABLE);
    if (err < 0) 
    {
        LOG_ERR("Failed to disable button interrupt: %d", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_DISABLE);
    if (err < 0) 
    {
        LOG_ERR("Failed to disable button 1 interrupt: %d", err);
        return err;
    }
}
