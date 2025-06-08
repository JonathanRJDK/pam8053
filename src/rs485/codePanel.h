#ifndef CODE_PANEL_H
#define CODE_PANEL_H

//Global macros used by the .c module which needs to easily be modified by the user
#define CODE_PANEL_INIT 0
#define CODE_PANEL_CODE_ENTERED 1
#define CODE_PANEL_CODE_ERROR 2
#define DOOR_OPEN 3
#define DOOR_CLOSED 4

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope

typedef struct
{
   uint8_t status;
   uint8_t errorCode;
} codepanelEvent;

typedef void(*codePanelEventHandlerCb)(const codepanelEvent *event);

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int CodePanelInit(codePanelEventHandlerCb codePanelEventHandler, uint8_t channel);

int CodePanelStart(uint8_t channel);

int CodePanelSetPasscode(char *passcode);

#ifdef __cplusplus
}
#endif

#endif //CODE_PANEL_H
