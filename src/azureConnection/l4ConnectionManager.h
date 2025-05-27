#ifndef L4_CONNECTION_MANAGER_H
#define L4_CONNECTION_MANAGER_H

//Global macros used by the .c module which needs to easily be modified by the user
#define NETWORK_CONNECTED_EVENT     1
#define NETWORK_DISCONNECTED_EVENT  2
#define NETWORK_RECONNECTED_EVENT   3

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope
typedef enum 
{
    L4_CNCT_MNG_NETWORK_CONNECTED,
    L4_CNCT_MNG_NETWORK_DISCONNECTED
} l4ConnectionManagerEventType;

typedef struct
{
    l4ConnectionManagerEventType event;
} l4ConnectionManagerEvent;

typedef void(*l4ConnectionManagerEventCb)(const l4ConnectionManagerEvent* event);


#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int L4ConnectionManagerNetworkInit(l4ConnectionManagerEventCb l4ConnectionManagerEventHandler);

int L4ConnectionManagerNetworkConnect(uint16_t timeoutSeconds);

int L4ConnectionManagerNetworkDisconnect();
#ifdef __cplusplus
}
#endif

#endif //L4_CONNECTION_MANAGER_H
