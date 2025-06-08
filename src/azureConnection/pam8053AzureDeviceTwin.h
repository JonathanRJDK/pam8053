#ifndef PAM8053_AZURE_DEVICE_TWIN_H
#define PAM8053_AZURE_DEVICE_TWIN_H

//Global macros used by the .c module which needs to easily be modified by the user
#define DT_MAX_NAME_LENGTH 64

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope
typedef struct
{
    uint16_t heartbeatInterval; //This is the heartbeat interval in seconds
    uint16_t powerMeterInterval; //This is the power meter interval in seconds
    uint8_t doorStatus; //This is the door status, 0 = closed, 1 = open, 2 = locked
    uint16_t doorCode;

    bool relay1Status; //Relay status, true = on, false = off
    bool relay2Status; //Relay status, true = on, false = off

    uint16_t alarm0Priority;
    char alarm0Name[64];

    uint16_t alarm1Priority;
    char alarm1Name[64];
} Pam8053DeviceTwinStruct;

typedef void(*Pam8053DeviceTwinEventHandlerCb)();

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
void Pam8053AzureDeviceTwinSetup(Pam8053DeviceTwinStruct *deviceTwinStruct, Pam8053DeviceTwinEventHandlerCb deviceTwinEventHandler);
void Pam8053DeviceTwinCb(const char *rxDeviceTwinBuf);

#ifdef __cplusplus
}
#endif

#endif //PAM8053_AZURE_DEVICE_TWIN_H
