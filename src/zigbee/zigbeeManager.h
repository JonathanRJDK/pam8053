#ifndef ZIGBEE_MANAGER_H
#define ZIGBEE_MANAGER_H

//Global macros used by the .c module which needs to easily be modified by the user
#define GLOBAL_MACRO1 1
#define GLOBAL_MACRO2 2

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope
typedef void(*zigbeeManagerEventHandlerCb)();

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int ZigbeeManager(zigbeeManagerEventHandlerCb *zigbeeManagerEventHandler);

int ZigbeeManagerConnect(uint16_t timeoutSeconds);

int ZigbeeManagerGetTemp(uint16_t timeoutSeconds);

int ZigbeeManagerManagerDisconnect();

#ifdef __cplusplus
}
#endif

#endif //ZIGBEE_MANAGER_H
