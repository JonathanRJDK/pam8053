#include "adcDts.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(adcDts, CONFIG_LOG_DEFAULT_LEVEL);

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

#define A1_CTRL_1 DT_ALIAS(a1ctrl1)
#define A1_CTRL_2 DT_ALIAS(a1ctrl2)
#define A2_CTRL_1 DT_ALIAS(a2ctrl1)
#define A2_CTRL_2 DT_ALIAS(a2ctrl2)

static const struct gpio_dt_spec a1ctrl1_pin = GPIO_DT_SPEC_GET(A1_CTRL_1, gpios);
static const struct gpio_dt_spec a1ctrl2_pin = GPIO_DT_SPEC_GET(A1_CTRL_2, gpios);
static const struct gpio_dt_spec a2ctrl1_pin = GPIO_DT_SPEC_GET(A2_CTRL_1, gpios);
static const struct gpio_dt_spec a2ctrl2_pin = GPIO_DT_SPEC_GET(A2_CTRL_2, gpios);

static const struct adc_dt_spec adcChannels[] = {DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)};

analogInputEventHandler inputEventHandler;

//Prototype of callback function for timer
extern void adcSampleTimerCb(struct k_timer *timer_id);
K_TIMER_DEFINE(adcSampleTimer,adcSampleTimerCb,NULL);

//Prototype of callback function for work handler
extern void adcWorkHandlerCb(struct k_work *work);
K_WORK_DEFINE(adcWorkHandler,adcWorkHandlerCb);

int16_t adcOutputValues[ARRAY_SIZE(adcChannels)];
bool valuesInitialized;

//Prototype of the init ctrl signal gpio
int initCtrlGpio();

int16_t buf;
struct adc_sequence sequence = 
{
	.buffer = &buf, 
	.buffer_size = sizeof(buf), //Buffer size in bytes, not number of samples
};

//Function that inits all the ADC channels defined in the devicetree overlay 
int AdcDtsInit(analogInputEventHandler eventHandler)
{
	int err;
	inputEventHandler = eventHandler;

	for(int i = 0; i < ARRAY_SIZE(adcChannels);i++)
	{
		if (!adc_is_ready_dt(&adcChannels[i])) 
		{
			LOG_ERR("ADC controller device %s not ready", adcChannels[i].dev->name);
			return -1;
		}

		err = adc_channel_setup_dt(&adcChannels[i]);
		if (err < 0) 
		{
			LOG_ERR("Could not setup channel #%d (Error: %d)", i, err);
			return -1;
		}
		LOG_INF("ADC Channels initialized:%d",adcChannels[i].channel_id);
	}

	initCtrlGpio();
	return 0;
}

void adcWorkHandlerCb(struct k_work *work)
{
	int err;

	//Read the ADC value from all channels initialized
	for(int i = 0; i< ARRAY_SIZE(adcChannels);i++)
	{
		err = adc_sequence_init_dt(&adcChannels[i], &sequence);
		if (err < 0) 
		{
			LOG_ERR("Could not init sequence on channel:%d",i);
		}

		err = adc_read(adcChannels[i].dev, &sequence);
		if (err < 0) 
		{
			LOG_ERR("Could not read channel#%d (Error: %d)", i, err);
		}

		// TODO: Investigate why we get negative values in the range -10 to 0
		if (buf < 0)
		{
			buf = 0;
		}
		
		// TODO: Consider threashodd
		if (adcOutputValues[i] != buf || valuesInitialized == false)
		{
			adcOutputValues[i] = (int)buf;
			if (inputEventHandler !=  NULL)
			{
				AnalogInputEvent ev;

				ev.event = valuesInitialized ? ANALOG_INPUT_CHANGED : ANALOG_INPUT_INIT_VALUE;
				ev.inputNo = i; // TOOD: Consider??? adcChannels[i].channel_id;
				ev.value = adcOutputValues[i];
				(*inputEventHandler)(&ev);
			}		
		}
	}	
	valuesInitialized = true;
}

//Function that runs through the output buffer and gets the latest value for a given channel
//This is done by checking the channel id in the adcChannels array, thereby finding a given channels position in the output array
//Usefull incase the devicetree declaration of ADC channels isn't in numerical order
//
//adcChannel is the channel to check (channel 0-7)
int AdcDtsGetSample(uint8_t channelNumber)
{
	if (channelNumber >= ARRAY_SIZE(adcChannels))
	{
		LOG_ERR("Invalid channel number");
		return -1;
	}
	return adcOutputValues[channelNumber];
/*	
	for(int i = 0; i< ARRAY_SIZE(adcChannels); i++)
	{
		if(adcChannels[i].channel_id == adcChannel)
		{
			return adcOutputValues[i];
		}
	}
	return -1;
*/
}

void adcSampleTimerCb(struct k_timer *timer_id)
{
	k_work_submit(&adcWorkHandler);
}


void adcTurnOff()
{
	k_timer_stop(&adcSampleTimer);
	LOG_INF("ADC turned off (sample timer stopped)");
}

//This function is only for when the adcInit has been called, and then the adcTurnOff was called
void adcTurnOn()
{
	LOG_INF("ADC turned on (sample timer starting)");
	k_timer_start(&adcSampleTimer, K_MSEC(ADC_SAMPLE_TIME_MS), K_MSEC(ADC_SAMPLE_TIME_MS));	
}

int initCtrlGpio()
{
	int err;

	if (!gpio_is_ready_dt(&a1ctrl1_pin)) 
	{
		LOG_ERR("Gpio not ready");
		return -1;
	}
	if (!gpio_is_ready_dt(&a1ctrl2_pin)) 
	{
		LOG_ERR("Gpio not ready");
		return -1;
	}
	
	if (!gpio_is_ready_dt(&a2ctrl1_pin)) 
	{
		LOG_ERR("Gpio not ready");
		return -1;
	}
	if (!gpio_is_ready_dt(&a2ctrl2_pin)) 
	{
		LOG_ERR("Gpio not ready");
		return -1;
	}
	
	err = gpio_pin_configure_dt(&a1ctrl1_pin, GPIO_OUTPUT_ACTIVE);
	if (err < 0) 
	{
		LOG_ERR("Config failed");
		return -1;
	}
	err = gpio_pin_configure_dt(&a1ctrl2_pin, GPIO_OUTPUT_ACTIVE);
	if (err < 0) 
	{
		LOG_ERR("Config failed");
		return -1;
	}
	
	err = gpio_pin_configure_dt(&a2ctrl1_pin, GPIO_OUTPUT_ACTIVE);
	if (err < 0) 
	{
		LOG_ERR("Config failed");
		return -1;
	}
	err = gpio_pin_configure_dt(&a2ctrl2_pin, GPIO_OUTPUT_ACTIVE);
	if (err < 0) 
	{
		LOG_ERR("Config failed");
		return -1;
	}
	
	return 0;
}


int AdcDtsSetCtrl1(int inputChannel)
{
	int err;
	switch(inputChannel)
	{
		case 0:
			err = gpio_pin_set_dt(&a1ctrl1_pin,true);
			if (err != 0)  	
			{
				LOG_ERR("Pin set failed");
				return -1;
			}
		break;

		case 1:
			err = gpio_pin_set_dt(&a2ctrl1_pin,true);
			if (err != 0) 
			{
				LOG_ERR("Pin set failed");
				return -1;
			}
		break;

		default:
			LOG_ERR("Unknown input channel number!");
			return -1;
		break;
	}
	return 0;
}

int AdcDtsSetCtrl2(int inputChannel)
{
	int err;
	switch(inputChannel)
	{
		case 0:
			err = gpio_pin_set_dt(&a1ctrl2_pin,true);
			if (err != 0)  	
			{
				LOG_ERR("Pin set failed");
				return -1;
			}
		break;

		case 1:
			err = gpio_pin_set_dt(&a2ctrl2_pin,true);
			if (err != 0) 
			{
				LOG_ERR("Pin set failed");
				return -1;
			}
		break;

		default:
			LOG_ERR("Unknown input channel number!");
			return -1;
		break;
	}
	return 0;
}
