#include "universalAlarmInput.h"
#include "stdint.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "adcDts.h"

LOG_MODULE_REGISTER(universalInputs, CONFIG_LOG_DEFAULT_LEVEL);

#define DIGITAL_THRESHOLD 100

static InputEventHandlerFunc pInputEventHandler;
static bool initDone = 0;

static InputMode inputMode[MAX_UIE_INPUTS];
static InputValue currentValue[MAX_UIE_INPUTS];

static uint16_t inputReference;			// Reference value for the input when calculating impedance values (pull up voltage)

// TODO: Fix this, 
extern int8_t alarmIdGlobal;

void HandleInputEvent(const AnalogInputEvent* pEvent)
{
	InputEvent inputEvent = 
	{
		.event = UIE_NULL
	};

	// OBS: It is important that inputNo has been validated before calling this function
	switch (inputMode[pEvent->inputNo])
	{
		case UIM_UNDEFINED:
			// Unconfigured input
			break;

		case  UIM_DIGITAL_INPUT_NO:
			bool noValue = pEvent->value > DIGITAL_THRESHOLD ? false : true;
			if (noValue != currentValue[pEvent->inputNo].bValue || pEvent->event == ANALOG_INPUT_INIT_VALUE)
			{
				currentValue[pEvent->inputNo].bValue = noValue;

				LOG_INF("Input %d NO changed to %s by analog value %d", pEvent->inputNo, noValue == true ? "Active" : "Inactive", pEvent->value);
				inputEvent.value.bValue = noValue;
				inputEvent.event = UIE_INPUT_CHANGED;
			}
			break;

		case UIM_DIGITAL_INPUT_NC:
			bool ncValue = pEvent->value > DIGITAL_THRESHOLD ? true : false;
			if (ncValue != currentValue[pEvent->inputNo].bValue || pEvent->event == ANALOG_INPUT_INIT_VALUE)
			{
				currentValue[pEvent->inputNo].bValue = ncValue;
				
				LOG_INF("Input %d NC changed to %s by analog value %d", pEvent->inputNo, ncValue == true ? "Active" : "Inactive", pEvent->value);
				inputEvent.value.bValue = ncValue;
				inputEvent.event = UIE_INPUT_CHANGED;
			}
			break;

		default:
			LOG_ERR("Unexpected mode for input number %d", pEvent->inputNo);
			break;
	}
	
	if (inputEvent.event != UIE_NULL)
	{
		inputEvent.inputNo = pEvent->inputNo;
		inputEvent.mode = inputMode[pEvent->inputNo];
		(*pInputEventHandler)(&inputEvent);
	}
}

static void AdcInputChangedCb(const AnalogInputEvent* event)
{
	if (initDone == 1)
	{
      if (pInputEventHandler != NULL)
      {
			if (event->inputNo == REFERENCE_INPUT)
			{
				if (event->event == ANALOG_INPUT_INIT_VALUE || event->event == ANALOG_INPUT_CHANGED)
				{
					// TODO: Add filter
					inputReference = event->value;
				}
			}
			else
			{
				if (event->inputNo < MAX_UIE_INPUTS)
				{
					HandleInputEvent(event);
				}
				else
				{
					LOG_ERR("Invalid input number: %d", event->inputNo);
				}
			}
      }
	} 
   else
	{
		LOG_ERR("Init isn't completed, unable to handle inputs yet!");
	}
}


// Initialize the inputs
void UniversalAlarmInputInit(InputEventHandlerFunc eventHandler)
{
   int result;

	LOG_INF("Initializing inputs");

   pInputEventHandler = eventHandler;

	//Init the ADC with a callback function that needs to be called on a change on the input
	//This function also inits the ctrl signals, however they still need to be set using the: "setAlarmMode()" function
	result = AdcDtsInit(AdcInputChangedCb);
	if (result < 0) 
	{
		LOG_ERR("Could not init sequence");
      return;
	}

	//Set the alarm mode, see adc module for available modes
	// TODO: Set input type from configuration
/*
	result = setAlarmMode(UIM_DIGITAL_INPUT_NC, 1);
	if (result < 0) 
	{
		LOG_ERR("Could not init ctrl gpio");
	}

	result = setAlarmMode(UIM_DIGITAL_INPUT_NC,2);
	if (result < 0) 
	{
		LOG_ERR("Could not init ctrl gpio");
	}

	k_sleep(K_MSEC(10));
*/
	//Setup the different settings for each channel
	//AnalogInputSetInputSettings(1, 8, ACTIVE_HIGH, 100);
	//AnalogInputSetInputSettings(2, 8, ACTIVE_HIGH, 100);

	initDone = 1;
}

void UniversalAlarmInputStart()
{
	//Turn on the ADC
	AdcDtsTurnOn();
	LOG_INF("ADC turned on (sample timer started)");
}

void UniversalAlarmInputSetMode(uint8_t inputNo, InputMode mode)
{
	int err;

	if (inputNo < MAX_UIE_INPUTS)
	{
		LOG_INF("Setting mode for input %d to %d", inputNo, mode);

		inputMode[inputNo] = mode;
		switch(mode)
		{
			case UIM_DIGITAL_INPUT_NO:
				err = AdcDtsSetCtrl1(inputNo);
			case UIM_DIGITAL_INPUT_NC:
				err = AdcDtsSetCtrl1(inputNo);
				break;
	
			case UIM_SUPERVISED_INPUT_PARALLEL:
				err = AdcDtsSetCtrl1(inputNo);
				break;
	
			case UIM_SUPERVISED_INPUT_SERIAL:
				err = AdcDtsSetCtrl1(inputNo);
				break;
	
			case UIM_SUPERVISED_INPUT_SERIAL_PARALLEL:
				err = AdcDtsSetCtrl1(inputNo);
				break;
	
			case UIM_VOLTAGE_INPUT:
				err = AdcDtsSetCtrl2(inputNo);
			break;
	
			case UIM_TEMPERATURE_NTC:
			case UIM_TEMPERATURE_PTC:
				err = AdcDtsSetCtrl1(inputNo);
				err = AdcDtsSetCtrl2(inputNo);
				break;
	
			default:
				LOG_WRN("Failed to set mode for input %d, invalid mode: %d", inputNo, mode);
				err = -1;
				break;
		}
		if (err != 0)
		{
			LOG_ERR("Failed to set mode for input %d, error: %d", inputNo, err);
		}
	}
	else
	{
		LOG_ERR("Invalid input number: %d", inputNo);
		return;
	}
}

