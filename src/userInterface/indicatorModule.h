#ifndef INDICATOR_MODULE_H
#define INDICATOR_MODULE_H

//Global macros used by the .c module which needs to easily be modified by the user
#define GLOBAL_MACRO1 1
#define GLOBAL_MACRO2 2

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int IndicatorModuleInit();

int IndicatorModuleStart();

int IndicatorModuleLedOn(uint8_t ledNumber);

int IndicatorModuleLedBlinkFast(uint8_t ledNumber);

int IndicatorModuleLedBlinkSlow(uint8_t ledNumber);

int IndicatorModuleLedColor(uint8_t rValue, uint8_t gValue, uint8_t bValue);

int IndicatorModuleLedOff(uint8_t ledNumber);

#ifdef __cplusplus
}
#endif

#endif //INDICATOR_MODULE_H
