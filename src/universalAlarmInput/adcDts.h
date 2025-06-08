#ifndef ADC_DTS_H
#define ADC_DTS_H

//Global macros used by the .c module which needs to easily be modified by the user
#define ADC_SAMPLE_TIME_MS 100
#define DEBOUNCE_TIMER_PERIOD_MS 100

#define ACTIVE_HIGH 0
#define ACTIVE_LOW  1


//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
//Global variables that needs to be accessed outside the modules scope
typedef enum 
{
    ANALOG_INPUT_INIT_VALUE,
    ANALOG_INPUT_CHANGED
} AnalogInputEventType;

typedef struct
{
    AnalogInputEventType event;
    uint8_t inputNo;
    uint16_t value;
} AnalogInputEvent;

typedef void(*analogInputEventHandler)(const AnalogInputEvent* event);

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int AdcDtsInit(analogInputEventHandler eventHandler);

int AdcDtsStart();

int AdcDtsGetSample(uint8_t channelNumber);

//This function turns off the timers needed for the ADC and debounce to work
void AdcDtsTurnOff();

//This function turns on the timers needed for the ADC and debounce to work
void AdcDtsTurnOn();

int AdcDtsSetCtrl1(int inputChannel);
int AdcDtsSetCtrl2(int inputChannel);


#ifdef __cplusplus
}
#endif

#endif //ADC_DTS_H
