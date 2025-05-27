#ifndef BUTTON_MODULE_H
#define BUTTON_MODULE_H

//Global macros used by the .c module which needs to easily be modified by the user
#define GLOBAL_MACRO1 1
#define GLOBAL_MACRO2 2

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope
typedef void(*buttonModuleEventHandlerCb)();

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int ButtonModuleInit(buttonModuleEventHandlerCb *buttonModuleEventHandler);

int ButtonModuleStart();

#ifdef __cplusplus
}
#endif

#endif //BUTTON_MODULE_H
