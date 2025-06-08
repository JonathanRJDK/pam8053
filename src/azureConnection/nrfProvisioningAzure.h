#ifndef NRF_PROVISIONING_AZURE_H
#define NRF_PROVISIONING_AZURE_H

//Global macros used by the .c module which needs to easily be modified by the user
#define MAX_BUFFER_LENGTH       2000

#define SOFTSIM_PROFILE_SIZE    190

#define DEVICE_ID_MAX_SIZE      64
#define SERIAL_NO_MAX_SIZE      64
#define ID_SCOPE_MAX_SIZE       64

#define CA_CERT_MAIN_SIZE       2000
#define CA_CERT_SECONDARY_SIZE  2000
#define CLIENT_CERT_SIZE        2000
#define PRIVATE_KEY_SIZE        2000

//The security tags used for storing the Azure certificates. This must match the declared values in prj.conf
#define SEC_TAG_MAIN            CONFIG_MQTT_HELPER_SEC_TAG
#define SEC_TAG_SECONDARY       CONFIG_MQTT_HELPER_SECONDARY_SEC_TAG

//Macros for the AT%CMNG= command:
//CMNG_COMMAND_X is the op code for a given command
#define CMNG_COMMAND_WRITE      0
#define CMNG_COMMAND_READ       1
#define CMNG_COMMAND_DELETE     3

//CMNG_COMMAND_X is the value for a given type of certificate/key
#define CMNG_TYPE_CA_CERT       0
#define CMNG_TYPE_CLIENT_CERT   1
#define CMNG_TYPE_PRIVATE_KEY   2
//Multiple certificates and keys can be stored under different security tags
//The default configuration for this application is that a main CA cert, is stored with a client cert and private key at security tag 1
//A second CA cert is also stored at security tag 2. This is per recommendation from Azure (Might not be needed in the future!)

#define TIME_TO_CHANGE_PROVISIONED_DATA  10 //Time to wait for the user to change the provisioned data, if not changed it will be kept

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <stdint.h>
#include <stdbool.h>
#include <net/azure_iot_hub.h>
#include <net/azure_iot_hub_dps.h>

//Global variables that needs to be accessed outside the modules scope

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int NrfProvisioningAzureRunProvisioningCheck(char *deviceIdBuf, char *idScopeBuf, char *serialNoBuf, uint16_t timeToChangeProvData);

#ifdef __cplusplus
}
#endif

#endif //NRF_PROVISIONING_AZURE_H
