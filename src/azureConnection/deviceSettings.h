#ifndef DEVICE_SETTINGS_H
#define DEVICE_SETTINGS_H

//Global macros used by the .c module which needs to easily be modified by the user
#define DEVICE_SETTINGS_KEY "device"
#define DEVICE_SETTINGS_SCOPE_ID_KEY "scopeId"
#define DEVICE_SETTINGS_DEVICE_ID_KEY "deviceId"
#define DEVICE_SETTINGS_SERIAL_NO_KEY "serialNo"

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

//Global variables that needs to be accessed outside the modules scope
typedef enum
{
   DEVICE_SETTINGS_STATUS_OK,
   DEVICE_SETTINGS_STATUS_ERROR
} DeviceSettingsStatus;

typedef void (*DeviceSettingsHandlerFunc)(DeviceSettingsStatus status);

typedef struct
{
	/* Pointer to buffer. */
	char *ptr;
	/* Size of buffer. */
	size_t size;
} DeviceSettingsBuffer;

typedef struct 
{
   DeviceSettingsHandlerFunc handler;
} DeviceSettings;

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int DeviceSettingsInit(DeviceSettings* settings);

int DeviceSettingsStart(void);

int DeviceSettingsGetScopeId(DeviceSettingsBuffer* pBuffer);
int DeviceSettingsGetDeviceId(DeviceSettingsBuffer* pBuffer);
int DeviceSettingsGetSerialNo(DeviceSettingsBuffer* pBuffer);

int DeviceSettingsDelete(void);

int DeviceSettingsSaveScopeId(const char *newScopeId, size_t newScopeIdLength);
int DeviceSettingsSaveDeviceId(const char *newScopeId, size_t newScopeIdLength);
int DeviceSettingsSaveSerialNo(const char *newScopeId, size_t newScopeIdLength);

#ifdef __cplusplus
}
#endif

#endif //DEVICE_SETTINGS_H
