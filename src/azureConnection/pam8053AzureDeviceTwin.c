#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <cJSON.h>

#include <net/azure_iot_hub.h>

#include "azureManager.h"
#include "l4ConnectionManager.h"
#include "pam8053AzureDeviceTwin.h"
#include "modemCommunicator.h"
#include "deviceSettings.h"

LOG_MODULE_REGISTER(Pam8053AzureDeviceTwin, LOG_LEVEL_INF);

static Pam8053DeviceTwinStruct *pam8053DTStruct;
Pam8053DeviceTwinEventHandlerCb dtEventHandler;

static K_SEM_DEFINE(twinRxAndTx, 1, 1);

//ToDo make a more elegant way of making objects in the callback function

//Prototype function for report function
void Pam8053TwinReportWork();

void Pam8053AzureDeviceTwinSetup(Pam8053DeviceTwinStruct *deviceTwinStruct, Pam8053DeviceTwinEventHandlerCb deviceTwinEventHandler)
{
    pam8053DTStruct = deviceTwinStruct;
	dtEventHandler = deviceTwinEventHandler;
}


void Pam8053DeviceTwinCb(const char *rxDeviceTwinBuf)
{
	//Take the semaphore to ensure that only one thread is accessing the device twin data at a time
	k_sem_take(&twinRxAndTx, K_NO_WAIT);

    cJSON *rootObj; 
	cJSON *desiredObj; 
	cJSON *telemetryConfigObj;
		cJSON *heartbeatTelemetryConfigObj; 
		cJSON *powerMeterTelemetryConfigObj;

    cJSON *doorStatusObj;

	cJSON *u0ConfigObj;
	cJSON *alarm0PriorityObj; 
	cJSON *alarm0NameObj;

	cJSON *u1ConfigObj;
	cJSON *alarm1PriorityObj; 
	cJSON *alarm1NameObj;

    rootObj = cJSON_Parse(rxDeviceTwinBuf);
	if (rootObj == NULL) 
	{
		LOG_ERR("Could not parse properties object");
	}

	desiredObj = cJSON_GetObjectItem(rootObj, "desired");
	if (desiredObj == NULL) 
	{
		LOG_DBG("Incoming device twin document contains only the 'desired' object");
		desiredObj = rootObj;
	}

	//Get the different objects
	telemetryConfigObj = cJSON_GetObjectItem(desiredObj, "telemetryConfig");
	if (telemetryConfigObj == NULL) 
	{
		LOG_DBG("No 'telemetryConfig' object found in the device twin document");
	}

		heartbeatTelemetryConfigObj = cJSON_GetObjectItem(telemetryConfigObj, "heartbeatSendInterval");
		if (heartbeatTelemetryConfigObj == NULL) 
		{
			LOG_DBG("No 'sendInterval' object found in the device twin document");
		}

		powerMeterTelemetryConfigObj = cJSON_GetObjectItem(telemetryConfigObj, "powerMeterSendInterval");
		if (powerMeterTelemetryConfigObj == NULL) 
		{
			LOG_DBG("No 'sendInterval' object found in the device twin document");
		}

    doorStatusObj = cJSON_GetObjectItem(desiredObj, "doorStatus");
    if (doorStatusObj == NULL)
    {
        LOG_DBG("No 'doorStatus' object found in the device twin document");
    }

	u0ConfigObj = cJSON_GetObjectItem(desiredObj, "u0Config");
	if (u0ConfigObj == NULL) 
	{
		LOG_DBG("No 'u0Config' object found in the device twin document");
	}

		alarm0PriorityObj = cJSON_GetObjectItem(u0ConfigObj, "alarm0Priority");
		if (alarm0PriorityObj == NULL) 
		{
			LOG_DBG("No 'alarm0Priority' object found in the device twin document");
		}

		alarm0NameObj = cJSON_GetObjectItem(u0ConfigObj, "alarm0Name");
		if (alarm0NameObj == NULL) 
		{
			LOG_DBG("No 'alarm0Name' object found in the device twin document");
		}

	u1ConfigObj = cJSON_GetObjectItem(desiredObj, "u1Config");
	if (u1ConfigObj == NULL) 
	{
		LOG_DBG("No 'u1Config' object found in the device twin document");
	}

		alarm1PriorityObj = cJSON_GetObjectItem(u1ConfigObj, "alarm1Priority");
		if (alarm1PriorityObj == NULL) 
		{
			LOG_DBG("No 'alarm0Priority' object found in the device twin document");
		}

		alarm1NameObj = cJSON_GetObjectItem(u1ConfigObj, "alarm1Name");
		if (alarm1NameObj == NULL) 
		{
			LOG_DBG("No 'alarm1Name' object found in the device twin document");
		}

	//Tests that the objects are the expected types, and then save the value
	if (cJSON_IsNumber(heartbeatTelemetryConfigObj)) 
	{
		pam8053DTStruct->heartbeatInterval = heartbeatTelemetryConfigObj->valueint;
	}
	else 
	{
		LOG_ERR("Invalid heartbeat interval format received");
	}

	if (cJSON_IsNumber(powerMeterTelemetryConfigObj)) 
	{
		pam8053DTStruct->powerMeterInterval = powerMeterTelemetryConfigObj->valueint;
	}
	else 
	{
		LOG_ERR("Invalid power meter interval format received");
	}

	if (cJSON_IsNumber(alarm0PriorityObj)) 
	{
		pam8053DTStruct->alarm0Priority = alarm0PriorityObj->valueint;
	}
	else 
	{
		LOG_ERR("Invalid alarm 2 priority format received");
	}

	if (cJSON_IsString(alarm0NameObj)) 
	{
		if(sizeof(alarm0NameObj->string) > DT_MAX_NAME_LENGTH)
		{
			LOG_ERR("Length beyond max string length");
		}
		else
		{
			strcpy(pam8053DTStruct->alarm0Name, alarm0NameObj->valuestring);
		}
	}
	else 
	{
		LOG_ERR("Invalid alarm 1 name format received");
	}

	if (cJSON_IsNumber(alarm1PriorityObj)) 
	{
		pam8053DTStruct->alarm1Priority = alarm1PriorityObj->valueint;
	}
	else 
	{
		LOG_ERR("Invalid alarm 2 priority format received");
	}

	if (cJSON_IsString(alarm1NameObj)) 
	{
		if(sizeof(alarm1NameObj->string) > DT_MAX_NAME_LENGTH)
		{
			LOG_ERR("Length beyond max string length");
		}
		else
		{
			strcpy(pam8053DTStruct->alarm1Name, alarm1NameObj->valuestring);
		}
	}
	else 
	{
		LOG_ERR("Invalid alarm 2 name format received");
	}

	cJSON_Delete(rootObj);
	
	//Report the device twin data to Azure IoT Hub
	Pam8053TwinReportWork();
}


void Pam8053TwinReportWork() 
{
    int err;

	uint8_t band = 0;
	
    // Buffer for JSON string
	char jsonString[512];
	char buf[1000];

	//Buffer for serial number
	char serialNo[32];

	ssize_t len;

    // Struct for Azure IoT Hub message
    struct azure_iot_hub_msg data = 
	{
        .topic.type = AZURE_IOT_HUB_TOPIC_TWIN_REPORTED,
        .payload.ptr = buf,
        .qos = MQTT_QOS_0_AT_MOST_ONCE,
    };

	// Get the serial number from the device settings
	err = DeviceSettingsGetSerialNo(serialNo);
	if(err < 0)
	{
		LOG_ERR("Failed to get serial number from device settings: %d", err);
		strcpy(serialNo, "UnknownSerialNo");
	}
	
    // Create the root JSON object
    cJSON *root = cJSON_CreateObject();
    if (!root) 
	{
        LOG_ERR("Failed to create root JSON object");
        return;
    }

	cJSON *telemetryConfigObj = cJSON_CreateObject();
	if (!telemetryConfigObj)
	{
		LOG_ERR("Failed to create telemetryConfig JSON object");
		cJSON_Delete(root);
		return;
	}

    cJSON *doorStatusObj = cJSON_CreateObject();
    if (!doorStatusObj)
    {
        LOG_ERR("Failed to create doorStatus JSON object");
        cJSON_Delete(root);
        return;
    }

	cJSON *u0ConfigObj = cJSON_CreateObject();
	if (!u0ConfigObj)
	{
		LOG_ERR("Failed to create 'u0Config' JSON object");
		cJSON_Delete(telemetryConfigObj);
		cJSON_Delete(root);
		return;
	}

	cJSON *u1ConfigObj = cJSON_CreateObject();
	if (!u1ConfigObj)
	{
		LOG_ERR("Failed to create u1Config JSON object");
		cJSON_Delete(u0ConfigObj);
		cJSON_Delete(telemetryConfigObj);
		cJSON_Delete(root);
		return;
	}

	cJSON *deviceInfo = cJSON_CreateObject();
    if (!deviceInfo) 
	{
        LOG_ERR("Failed to create root JSON object");
        return;
    }

	ModemCommunicatorAtCommandXcband(&band);

    //Add sendInterval to the JSON object  
	cJSON_AddItemToObject(root, "telemetryConfig", telemetryConfigObj);	
		cJSON_AddNumberToObject(telemetryConfigObj, "heartbeatSendInterval", pam8053DTStruct->heartbeatInterval);
		cJSON_AddNumberToObject(telemetryConfigObj, "powerMeterSendInterval", pam8053DTStruct->powerMeterInterval);

    //Add doorStatus to the JSON object
    cJSON_AddItemToObject(root, "doorStatus", doorStatusObj);
	cJSON_AddNumberToObject(doorStatusObj, "status", pam8053DTStruct->doorStatus);

	//Add u0Config to the JSON object
	cJSON_AddItemToObject(root, "u0Config", u0ConfigObj);
		cJSON_AddNumberToObject(u0ConfigObj, "alarm0Priority", pam8053DTStruct->alarm0Priority);
		cJSON_AddStringToObject(u0ConfigObj, "alarm0Name", pam8053DTStruct->alarm0Name);

	//Add u1Config to the JSON object
	cJSON_AddItemToObject(root, "u1Config", u1ConfigObj);
		cJSON_AddNumberToObject(u1ConfigObj, "alarm1Priority", pam8053DTStruct->alarm1Priority);
		cJSON_AddStringToObject(u1ConfigObj, "alarm1Name", pam8053DTStruct->alarm1Name);

	//Add deviceInfo to the JSON object
	cJSON_AddItemToObject(root, "deviceInfo", deviceInfo);
		cJSON_AddStringToObject(deviceInfo, "serialNo", serialNo);
		cJSON_AddNumberToObject(deviceInfo, "mobileBand", band);
		cJSON_AddStringToObject(deviceInfo, "version", CONFIG_AZURE_FOTA_APP_VERSION);
		cJSON_AddStringToObject(deviceInfo, "model", "PAM8002");

    
	cJSON_PrintPreallocated(root,jsonString,sizeof(jsonString),false);
	//Release resources
	cJSON_Delete(root);
	
	len = snprintk(buf, sizeof(buf),jsonString);
    data.payload.size = len;

    // Send the message to Azure IoT Hub
    err = azure_iot_hub_send(&data);
    if (err) 
	{
        LOG_ERR("Failed to send twin report: %d", err);
        cJSON_Delete(root);
        return;
    }

    LOG_INF("Twin report sent successfully");
    LOG_INF("New heartbeat interval has been saved: %d", pam8053DTStruct->heartbeatInterval);
	LOG_INF("New power meter interval has been saved: %d", pam8053DTStruct->powerMeterInterval);
    LOG_INF("New alarm 1 priority has been saved: %d", pam8053DTStruct->alarm0Priority);
    LOG_INF("New alarm 1 name has been saved: %s", pam8053DTStruct->alarm0Name);
    LOG_INF("New alarm 2 priority has been saved: %d", pam8053DTStruct->alarm1Priority);
    LOG_INF("New alarm 2 name has been saved: %s", pam8053DTStruct->alarm1Name);

    //Give the semaphore and trigger event handler, data should only be accessed at this time
    k_sem_give(&twinRxAndTx);
    (*dtEventHandler)();

    return;
}
	