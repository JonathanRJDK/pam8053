#ifndef AZURE_MANAGER_H
#define AZURE_MANAGER_H

//Global macros used by the .c module which needs to easily be modified by the user
#define NETWORK_CONNECTION_CONNECTED     1
#define NETWORK_CONNECTION_DISCONNECTED  2
#define NETWORK_CONNECTION_RECONNECTED   3

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>
#include <net/azure_iot_hub.h>
#include <net/azure_iot_hub_dps.h>

//Global variables that needs to be accessed outside the modules scope
typedef enum 
{
    AZURE_MNG_NETWORK_CONNECTED,
    AZURE_MNG_NETWORK_DISCONNECTED
} azureManagerEventType;

typedef struct
{
    azureManagerEventType event;
} azureManagerEvent;

//Callback function types for the Azure manager
typedef void(*azureEventHandlerCb)(const azureManagerEvent* event);

// Callback function type for device twin messages
typedef void(*deviceTwinHandlerCb)(const char *rxDeviceTwinBuf);

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int AzureManagerInit(azureEventHandlerCb azureEventHandler, deviceTwinHandlerCb deviceTwinHandler, char *deviceIdStr, char *idScopeStr);

int AzureManagerConnect();

int AzureManagerDisconnect();

int AzureManagerSendTelemetry(char *telemetryString);

#ifdef __cplusplus
}
#endif

#endif //AZURE_MANAGER_H
