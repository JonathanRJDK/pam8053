#ifndef BUTTON_MODULE_H
#define BUTTON_MODULE_H

//Global macros used by the .c module which needs to easily be modified by the user


//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope
typedef enum 
{
    BUTTON_INIT,
    BUTTON_PRESSED,
    BUTTON_RELEASED,
} buttonsHandlerStatus;

typedef struct
{
    buttonsHandlerStatus status;
    uint8_t buttonId;
} buttonsHandlerEvent;

typedef void(*buttonsHandlerEventCb)(const buttonsHandlerEvent* status);


#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int ButtonsHandlerInit(buttonsHandlerEventCb eventHandler);

int ButtonsHandlerStart(gpio_flags_t interruptMode);

int ButtonsHandlerStop(void);

#ifdef __cplusplus
}
#endif

#endif //BUTTON_MODULE_H
