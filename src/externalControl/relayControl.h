#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

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
int RelayControlInit();

int RelayControlStart();

int RelayControlRelayOn(uint8_t relayNumber);

int RelayControlRelayOff(uint8_t relayNumber);

#ifdef __cplusplus
}
#endif

#endif //RELAY_CONTROL_H
