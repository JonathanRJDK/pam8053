#include "deviceSettings.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

// Fix this dependency
#include <net/azure_iot_hub.h>
#include <net/azure_iot_hub_dps.h>
#include <azure/iot/az_iot_hub_client.h>

//#define STATIC_HANDLER_DECLARATION

LOG_MODULE_REGISTER(deviceSettings, LOG_LEVEL_ERR);

typedef struct 
{
   bool isInitialized;
   struct k_sem loaded;
   DeviceSettingsHandlerFunc cb;
   az_span scopeId;
   az_span deviceId;
   az_span serialNo;
} DeviceSettingsCtx;

static DeviceSettingsCtx ctx;

static bool serialSet = false;

static char serialNo[16]; 
static char deviceId[16];
static char scopeId[16]; //= CONFIG_AZURE_IOT_HUB_DPS_ID_SCOPE;

static int DeviceSettingHandler(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg);
static int DeviceSettingLoaded(void);

#ifdef STATIC_HANDLER_DECLARATION

SETTINGS_STATIC_HANDLER_DEFINE(deviceSettings, DEVICE_SETTINGS_KEY, NULL, DeviceSettingHandler, DeviceSettingLoaded, NULL);

#else	

/* dynamic main tree handler */
struct settings_handler deviceSettingsHandler = {
	.name = "device",
	.h_get = NULL,
	.h_set = DeviceSettingHandler,
	.h_commit = DeviceSettingLoaded,
	.h_export = NULL
};

#endif

int DeviceSettingHandler(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	const char *next;
	//size_t next_len;
	int rc = 0;

	if (settings_name_steq(name, DEVICE_SETTINGS_SERIAL_NO_KEY, &next) && !next) 
	{
		if (len > sizeof(serialNo)) 
		{
			return -EINVAL;
		}
		rc = read_cb(cb_arg, &serialNo, sizeof(serialNo));
		if (rc >= 0) 
		{
			LOG_INF("Device loaded serialNo: %s", serialNo);
         ctx.serialNo = az_span_create(serialNo, MIN(sizeof(serialNo), strlen(serialNo)));
			serialSet = true;
		}
	}
	else if (settings_name_steq(name, DEVICE_SETTINGS_DEVICE_ID_KEY, &next) && !next) 
	{
		if (len > sizeof(deviceId)) 
		{
			return -EINVAL;
		}
		rc = read_cb(cb_arg, &deviceId, sizeof(deviceId));
		if (rc >= 0) 
		{
			LOG_INF("Device loaded deviceId: %s", deviceId);
         ctx.deviceId = az_span_create(deviceId, MIN(sizeof(deviceId), strlen(deviceId)));
		}
	}
	else if (settings_name_steq(name, DEVICE_SETTINGS_SCOPE_ID_KEY, &next) && !next) 
	{
		if (len > sizeof(scopeId)) 
		{
			return -EINVAL;
		}
		rc = read_cb(cb_arg, &scopeId, sizeof(scopeId));
		if (rc >= 0) 
		{
			LOG_INF("Device loaded scopeId: %s", scopeId);
         ctx.scopeId = az_span_create(scopeId, MIN(sizeof(scopeId), strlen(scopeId)));
		}
	}
	return 0;
}

int DeviceSettingLoaded(void)
{
	LOG_INF("Loading all settings under <device> handler is done");
	if (!serialSet)
	{
		LOG_WRN("No serial number configured, storing new value");
		//These values should be changed and used on devices that where deployed without the NVS storage of device id and scope
		//This is no longer needed, since the device id and scope id is now stored in the NVS partition from the beginning
		
		//DeviceSettingsSaveDeviceId("PAM8002_1040", 12);
		//DeviceSettingsSaveSerialNo("0000-0000", 9);
		//DeviceSettingsSaveScopeId("0ne008A3851", 11);
		
	}

	k_sem_give(&ctx.loaded);
	return 0;
}

int DeviceSettingsGetScopeId(DeviceSettingsBuffer* pBuffer)
{
	size_t scopeIdLen = az_span_size(ctx.scopeId);
	char *scopeIdPtr = az_span_ptr(ctx.scopeId);

	if (pBuffer == NULL)
   {
		return -EINVAL;
	}

	if ((scopeIdLen == 0) || (scopeIdPtr == NULL)) 
   {
		LOG_DBG("No assigned scope id found");
		return -ENOENT;
	}

	pBuffer->ptr = scopeIdPtr;
	pBuffer->size = scopeIdLen;
	return 0;
}

int DeviceSettingsGetDeviceId(DeviceSettingsBuffer* pBuffer)
{
	size_t deviceIdLen = az_span_size(ctx.deviceId);
	char *deviceIdPtr = az_span_ptr(ctx.deviceId);

	if (pBuffer == NULL)
   {
		return -EINVAL;
	}

	if ((deviceIdLen == 0) || (deviceIdPtr == NULL)) 
   {
		LOG_DBG("No assigned deviceId found");
		return -ENOENT;
	}

	pBuffer->ptr = deviceIdPtr;
	pBuffer->size = deviceIdLen;
	return 0;
}

int DeviceSettingsGetSerialNo(DeviceSettingsBuffer* pBuffer)
{
	size_t serialNoLen = az_span_size(ctx.serialNo);
	char *serialNoPtr = az_span_ptr(ctx.serialNo);

	if (pBuffer == NULL)
   {
		return -EINVAL;
	}

	if ((serialNoLen == 0) || (serialNoPtr == NULL)) 
   {
		LOG_DBG("No assigned serialNo found");
		return -ENOENT;
	}

	pBuffer->ptr = serialNoPtr;
	pBuffer->size = serialNoLen;
	return 0;
}

int DeviceSettingsSaveScopeId(const char *newScopeId, size_t newScopeIdLength)
{
	int err;

	LOG_INF("Device settings, save new scopeId: %s", newScopeId);

	size_t scopeIdLength = MIN(sizeof(scopeId) - 1, newScopeIdLength);
	memcpy(scopeId, newScopeId, scopeIdLength);
	scopeId[scopeIdLength] = '\0';

	ctx.scopeId = az_span_create(scopeId, scopeIdLength);

	err = settings_save_one(DEVICE_SETTINGS_KEY "/" DEVICE_SETTINGS_SCOPE_ID_KEY, scopeId, scopeIdLength);
	if (err) 
	{
		LOG_ERR("Device settings_save_one failed (err %d)", err);
	}
}

int DeviceSettingsSaveDeviceId(const char *newDeviceId, size_t newDeviceIdLength)
{
	int err;

	LOG_INF("Device settings, save new deviceId: %s", newDeviceId);

	size_t deviceIdLength = MIN(sizeof(deviceId) - 1, newDeviceIdLength);
	memcpy(deviceId, newDeviceId, deviceIdLength);
	deviceId[deviceIdLength] = '\0';

	ctx.deviceId = az_span_create(deviceId, deviceIdLength);

	err = settings_save_one(DEVICE_SETTINGS_KEY "/" DEVICE_SETTINGS_DEVICE_ID_KEY, deviceId, deviceIdLength);
	if (err) 
	{
		LOG_ERR("Device settings_save_one failed (err %d)", err);
	}
}

int DeviceSettingsSaveSerialNo(const char *newSerialNo, size_t newSerialNoLength)
{
	int err;

	LOG_INF("Device settings, save new serialNo: %s", newSerialNo);

	size_t serialNoLength = MIN(sizeof(serialNo) - 1, newSerialNoLength);
	memcpy(serialNo, newSerialNo, serialNoLength);
	serialNo[serialNoLength] = '\0';

	err = settings_save_one(DEVICE_SETTINGS_KEY "/" DEVICE_SETTINGS_SERIAL_NO_KEY, serialNo, serialNoLength);
	if (err) 
	{
		LOG_ERR("Device settings_save_one failed (err %d)", err);
	}
}

int DeviceSettingsDelete(void)
{
	int err;

	LOG_INF("Device delete settings\n");
	err = settings_delete(DEVICE_SETTINGS_KEY "/" DEVICE_SETTINGS_SERIAL_NO_KEY);
	if (err) 
	{
		LOG_ERR("Device serialNo settings_delete failed (err %d)", err);
		return;
	}
	err = settings_delete(DEVICE_SETTINGS_KEY "/" DEVICE_SETTINGS_DEVICE_ID_KEY);
	if (err) 
	{
		LOG_ERR("Device deviceId settings_delete failed (err %d)", err);
		return;
	}
	err = settings_delete(DEVICE_SETTINGS_KEY "/" DEVICE_SETTINGS_SCOPE_ID_KEY);
	if (err) 
	{
		LOG_ERR("Device scopeId settings_delete failed (err %d)", err);
		return;
	}
}

int DeviceSettingsInit(DeviceSettings* settings)
{
	int err;

	if (ctx.isInitialized) 
	{
		return -EALREADY;
	}

	LOG_INF("Device settings initialize");

	ctx.isInitialized = true;
	k_sem_init(&ctx.loaded, 0, 1);

	err = settings_subsys_init();
	if (err) 
	{
		LOG_ERR("DeviceSettingsInitialize settings_subsys_init failed (err %d)", err);
		return err;
	}

#ifndef STATIC_HANDLER_DECLARATION	
	err = settings_register(&deviceSettingsHandler);
	if (err) 
	{
		LOG_ERR("device settings_register failed (err %d)", err);
		return err;
	}
#endif

   // Temporary fix to reset device settings
#ifdef CLEAR_DEVICE_SETTINGS
	DeviceSettingsDelete();
	azure_iot_hub_dps_reset();
#endif	

	LOG_INF("Loading device subtree");
	err = settings_load_subtree(DEVICE_SETTINGS_KEY);
	if (err) 
	{
		LOG_ERR("DeviceSettingsInitialize settings_load_subtree failed (err %d)", err);
		return err;
	}
   return 0;
}

int DeviceSettingsStart(void)
{
   int err;
   int result = 0;

	LOG_INF("Device settings start, wait for setting to be loaded");
   err = k_sem_take(&ctx.loaded, K_SECONDS(1));
   if (err) 
   {
      LOG_ERR("DeviceSettingsStart not starting (timeout)");
      result = -ETIMEDOUT;
   }
   return result;
}
