#ifndef UNIVERSAL_ALARM_INPUT_H
#define UNIVERSAL_ALARM_INPUT_H

//Global macros used by the .c module which needs to easily be modified by the user
// Number of universal inputs
#define MAX_UIE_INPUTS 2

// Which input no. is used for reference (not included in the normal universal inputs)
#define REFERENCE_INPUT 2

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <stdint.h>
#include <stdbool.h>

//Global variables that needs to be accessed outside the modules scope
// Universal input modes
typedef enum
{
   UIM_UNDEFINED,
   UIM_DIGITAL_INPUT_NO,
   UIM_DIGITAL_INPUT_NC,
   UIM_SUPERVISED_INPUT_PARALLEL,
   UIM_SUPERVISED_INPUT_SERIAL,
   UIM_SUPERVISED_INPUT_SERIAL_PARALLEL,
   UIM_VOLTAGE_INPUT,
   UIM_TEMPERATURE_NTC,
   UIM_TEMPERATURE_PTC
} InputMode;

// Universal input event types
typedef enum 
{
   UIE_NULL,
   UIE_INIT_VALUE,
   UIE_INPUT_CHANGED,
   UIE_INPUT_FAULT
} InputEventType;

typedef union universalInputsValue
{
   uint16_t uValue;
   int16_t iValue;
   uint8_t bValue;
} InputValue;

typedef struct
{
   uint8_t inputNo;
   InputEventType event;
   InputMode mode;
   InputValue value;
} InputEvent;

// Universal input event handler
typedef void(*InputEventHandlerFunc)(const InputEvent* event);

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
// Universal input event handler
typedef void(*InputEventHandlerFunc)(const InputEvent* event);

// Initialize the inputs
void UniversalAlarmInputInit(InputEventHandlerFunc eventHandler);

// Start processing the inputs
void UniversalAlarmInputStart();

// Set mode for the input
void UniversalAlarmInputSetMode(uint8_t inputNo, InputMode mode);

#ifdef __cplusplus
}
#endif

#endif //UNIVERSAL_ALARM_INPUT_H
