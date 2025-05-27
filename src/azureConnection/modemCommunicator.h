#ifndef MODEM_COMMUNICATOR_H
#define MODEM_COMMUNICATOR_H

//Global macros used by the .c module which needs to easily be modified by the user


//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int ModemCommunicatorInit();

int ModemCommunicatorAtCommandXcband(uint8_t *band);

int ModemCommunicatorAtCommandCesq(uint8_t *rxlev, uint8_t *ber, uint8_t *rscp, uint8_t *ecno, uint8_t *rsrq, uint8_t *rsrp);

#ifdef __cplusplus
}
#endif

#endif //MODEM_COMMUNICATOR_H
