#ifndef RS485_COMMUNICATION_H
#define RS485_COMMUNICATION_H

//Global macros used by the .c module which needs to easily be modified by the user
#define GLOBAL_MACRO1 1
#define GLOBAL_MACRO2 2

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope
typedef void(*Rs485CommunicationEventHandlerCb)();

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int Rs485CommunicationInit(Rs485CommunicationEventHandlerCb *Rs485CommunicationEventHandler, uint8_t channel);

int Rs485CommunicationStart(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif //RS485_COMMUNICATION_H
