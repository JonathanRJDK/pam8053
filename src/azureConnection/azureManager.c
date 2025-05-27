#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/dfu/mcuboot.h>
#include <net/azure_iot_hub.h>
#include <net/azure_iot_hub_dps.h>
#include <zephyr/logging/log.h>
#include <cJSON.h>
#include <cJSON_os.h>

#include "azureManager.h"
#include "deviceReboot.h"

LOG_MODULE_REGISTER(azureManager, LOG_LEVEL_DBG);

#define EVENT_INTERVAL		60
#define RECV_BUF_SIZE		1024
#define APP_WORK_Q_STACK_SIZE	KB(8)

#define MAX_CONNECTION_RETRY 20

static struct method_data 
{
	struct k_work work;
	char request_id[8];
	char name[32];
	char payload[200];
} method_data;

azureEventHandlerCb azureManagerHandler;
deviceTwinHandlerCb dTHandler;

static struct k_work_delayable reboot_work;

uint8_t *desiredObjectName;

static char recv_buf[RECV_BUF_SIZE];
static void direct_method_handler(struct k_work *work);
static K_SEM_DEFINE(recv_buf_sem, 1, 1);
static atomic_t event_interval = EVENT_INTERVAL;

static uint16_t connectionRetires;

static bool dps_was_successful;
static K_SEM_DEFINE(dps_done_sem, 0, 1);

static K_THREAD_STACK_DEFINE(application_stack_area, APP_WORK_Q_STACK_SIZE);
static struct k_work_q application_work_q;

//These two strings are used for DPS, these should be provided at runtime and is therefore provided from the external provisioning module...

static char hostname[128];
static char device_id[128];
static char id_scope_buf[128];

static struct azure_iot_hub_config cfg = 
{
	.device_id = 
    {
		.ptr = device_id,
	},
	.hostname = 
    {
		.ptr = hostname,
	},
	.use_dps = false, //Very important to have this flag as false when useing runtime provided id_scope
};



//Function prototypes
int AzureInit(azureEventHandlerCb eventHandler, deviceTwinHandlerCb deviceTwinHandler, char *device_id_str, char *id_scope_str);
int AzureConnect();
int AzureDisconnect();
int AzureSendTelemetry(char* telemetryString);

//Returns a positive integer if the new interval can be parsed, otherwise -1
static int event_interval_get(char *buf)
{
	struct cJSON *root_obj, *desired_obj, *event_interval_obj;
	int new_interval = -1;

	root_obj = cJSON_Parse(buf);
	if (root_obj == NULL) {
		LOG_ERR("Could not parse properties object");
		return -1;
	}

	/* If the incoming buffer is a notification from the cloud about changes
	 * made to the device twin's "desired" properties, the root object will
	 * only contain the newly changed properties, and can be treated as if
	 * it is the "desired" object.
	 * If the incoming is the response to a request to receive the device
	 * twin, it will contain a "desired" object and a "reported" object,
	 * and we need to access that object instead of the root.
	 */
	desired_obj = cJSON_GetObjectItem(root_obj, "desired");
	if (desired_obj == NULL) 
	{
		LOG_DBG("Incoming device twin document contains only the 'desired' object");
		desired_obj = root_obj;
	}

	/* Update only recognized properties. */
	event_interval_obj = cJSON_GetObjectItem(desired_obj, "telemetryInterval");
	if (event_interval_obj == NULL) 
	{
		LOG_DBG("No 'telemetryInterval' object found in the device twin document");
		goto clean_exit;
	}

	if (cJSON_IsString(event_interval_obj)) 
	{
		new_interval = atoi(event_interval_obj->valuestring);
	} else if (cJSON_IsNumber(event_interval_obj)) 
	{
		new_interval = event_interval_obj->valueint;
	} else 
	{
		LOG_WRN("Invalid telemetry interval format received");
		goto clean_exit;
	}

clean_exit:
	cJSON_Delete(root_obj);
	k_sem_give(&recv_buf_sem);

	return new_interval;
}

static void on_evt_direct_method(struct azure_iot_hub_method *method)
{
	size_t request_id_len = MIN(sizeof(method_data.request_id) - 1, method->request_id.size);
	size_t name_len = MIN(sizeof(method_data.name) - 1, method->name.size);

	LOG_INF("Method name: %.*s", method->name.size, method->name.ptr);
	LOG_INF("Payload: %.*s", method->payload.size, method->payload.ptr);

	memcpy(method_data.request_id, method->request_id.ptr, request_id_len);

	method_data.request_id[request_id_len] = '\0';

	memcpy(method_data.name, method->name.ptr, name_len);
	method_data.name[name_len] = '\0';

	snprintk(method_data.payload, sizeof(method_data.payload),
		 "%.*s", method->payload.size, method->payload.ptr);

	k_work_submit_to_queue(&application_work_q, &method_data.work);
}

static void reboot_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	DeviceRebootNormal();
}


//Used for applying a newly received send interval, not currently used anywhere in this manager!
static void event_interval_apply(int interval)
{
	if (interval <= 0) 
	{
		return;
	}

	atomic_set(&event_interval, interval);
}

static void azure_event_handler(struct azure_iot_hub_evt *const evt)
{
	azureManagerEvent ev;

	switch (evt->type) 
	{
	case AZURE_IOT_HUB_EVT_CONNECTING:
		connectionRetires++;
		LOG_INF("AZURE_IOT_HUB_EVT_CONNECTING (retry %d)", connectionRetires);
		if(connectionRetires > MAX_CONNECTION_RETRY)
		{
			LOG_ERR("Max connection retries reached, rebooting device");
			DeviceRebootError();
		}	
		break;

	case AZURE_IOT_HUB_EVT_CONNECTED:
		connectionRetires = 0;
		LOG_INF("AZURE_IOT_HUB_EVT_CONNECTED");
		if (azureManagerHandler != NULL)
		{
			ev.event=AZURE_MNG_NETWORK_CONNECTED;
			(*azureManagerHandler)(&ev); 
		}	
		break;

	case AZURE_IOT_HUB_EVT_CONNECTION_FAILED:
		LOG_INF("AZURE_IOT_HUB_EVT_CONNECTION_FAILED");
		LOG_INF("Error code received from IoT Hub: %d", evt->data.err);
	break;

	case AZURE_IOT_HUB_EVT_DISCONNECTED:
		LOG_INF("AZURE_IOT_HUB_EVT_DISCONNECTED");
		if (azureManagerHandler != NULL)
		{
			ev.event=AZURE_MNG_NETWORK_DISCONNECTED;
			(*azureManagerHandler)(&ev); 
		}	
		break;

	case AZURE_IOT_HUB_EVT_READY:
		LOG_INF("AZURE_IOT_HUB_EVT_READY");
		boot_write_img_confirmed();
//		ev.event=AZURE_MNG_NETWORK_CONNECTED;
//		(*azureManagerHandler)(&ev); //Trigger callback function
	break;

	case AZURE_IOT_HUB_EVT_DATA_RECEIVED:
		LOG_INF("AZURE_IOT_HUB_EVT_DATA_RECEIVED");
		LOG_INF("Received payload: %.*s",evt->data.msg.payload.size, evt->data.msg.payload.ptr);
	break;

	case AZURE_IOT_HUB_EVT_TWIN_RECEIVED:
		LOG_INF("AZURE_IOT_HUB_EVT_TWIN_RECEIVED");
		(*dTHandler)(evt->data.msg.payload.ptr);
	break;

	case AZURE_IOT_HUB_EVT_TWIN_DESIRED_RECEIVED:
		LOG_INF("AZURE_IOT_HUB_EVT_TWIN_DESIRED_RECEIVED");
		(*dTHandler)(evt->data.msg.payload.ptr);
	break;

	case AZURE_IOT_HUB_EVT_DIRECT_METHOD:
		LOG_INF("AZURE_IOT_HUB_EVT_DIRECT_METHOD");
		on_evt_direct_method(&evt->data.method);
	break;

	case AZURE_IOT_HUB_EVT_TWIN_RESULT_SUCCESS:
		LOG_INF("AZURE_IOT_HUB_EVT_TWIN_RESULT_SUCCESS, ID: %.*s",
			evt->data.result.request_id.size, evt->data.result.request_id.ptr);
	break;

	case AZURE_IOT_HUB_EVT_TWIN_RESULT_FAIL:
		LOG_INF("AZURE_IOT_HUB_EVT_TWIN_RESULT_FAIL, ID %.*s, status %d",
			evt->data.result.request_id.size,
			evt->data.result.request_id.ptr,
			evt->data.result.status);
	break;

	case AZURE_IOT_HUB_EVT_PUBACK:
		LOG_INF("AZURE_IOT_HUB_EVT_PUBACK");
	break;

	case AZURE_IOT_HUB_EVT_FOTA_START:
		LOG_INF("AZURE_IOT_HUB_EVT_FOTA_START");
	break;

	case AZURE_IOT_HUB_EVT_FOTA_DONE:
		LOG_INF("AZURE_IOT_HUB_EVT_FOTA_DONE");
		LOG_INF("The device will reboot in 5 seconds to apply update");
		k_work_schedule(&reboot_work, K_SECONDS(5));
	break;

	case AZURE_IOT_HUB_EVT_FOTA_ERASE_PENDING:
		LOG_INF("AZURE_IOT_HUB_EVT_FOTA_ERASE_PENDING");
	break;

	case AZURE_IOT_HUB_EVT_FOTA_ERASE_DONE:
		LOG_INF("AZURE_IOT_HUB_EVT_FOTA_ERASE_DONE");
	break;
        
	case AZURE_IOT_HUB_EVT_FOTA_ERROR:
		LOG_ERR("AZURE_IOT_HUB_EVT_FOTA_ERROR: FOTA failed");
	break;

	case AZURE_IOT_HUB_EVT_ERROR:
		LOG_INF("AZURE_IOT_HUB_EVT_ERROR");
	break;

	default:
		LOG_ERR("Unknown Azure IoT Hub event type: %d", evt->type);
	break;
	}
}

//ToDo this handler should instead be a external module which is up to the different applications to implement
//See device twin application for inspiration
static void direct_method_handler(struct k_work *work)
{
	int err;

	/* Status code 200 indicates successful execution of direct method. */
	struct azure_iot_hub_result result = 
	{
		.request_id = 
		{
			.ptr = method_data.request_id,
			.size = strlen(method_data.request_id),
		},
		.status = 200,
		.payload.ptr = NULL, // response,
		.payload.size = 0 // sizeof(response) - 1,
	};	

	LOG_INF("direct_method_handler, method_data.name: %s", method_data.name);

	if (strcmp(method_data.name, "Reboot") == 0) 
	{
		LOG_INF("Rebooting device");
		k_work_schedule(&reboot_work, K_SECONDS(1));
	}

	//From here is the handling of a direct method. This is not currently used anywhere in this manager!
	//Below is the commented original example provided with the azure_iot_hub example
/*
	bool led_state = strncmp(method_data.payload, "0", 1) ? 1 : 0;

	if (strcmp(method_data.name, "led") != 0) 
	{
		LOG_INF("Unknown direct method");
		return;
	}

	err = azure_iot_hub_method_respond(&result);
	if (err) 
	{
		LOG_ERR("Failed to send direct method response");
	}
*/
}

int checkAndAssignJsonObjectInt(struct cJSON *jsonObj, struct cJSON *jsonObjRoot)
{
	int value;

	jsonObj = cJSON_GetObjectItem(jsonObjRoot, "telemetryInterval");
	if (jsonObj == NULL) 
	{
		LOG_DBG("No '%s' object found in the device twin document",jsonObj->string);
	}
	
	if (cJSON_IsString(jsonObj))
	{
		value = atoi(jsonObj->valuestring);
	} 
	else if (cJSON_IsNumber(jsonObj)) 
	{
		value = jsonObj->valueint;
	} 
	else 
	{
		LOG_WRN("Invalid format received");
	}

	return value;
}


static void work_init(void)
{
	k_work_init(&method_data.work, direct_method_handler);
	k_work_init_delayable(&reboot_work, reboot_work_fn);
	k_work_queue_start(&application_work_q, application_stack_area, K_THREAD_STACK_SIZEOF(application_stack_area), K_HIGHEST_APPLICATION_THREAD_PRIO, NULL);
}

static void dps_handler(enum azure_iot_hub_dps_reg_status status)
{
	switch (status) 
    {
	case AZURE_IOT_HUB_DPS_REG_STATUS_NOT_STARTED:
		LOG_INF("DPS registration status: AZURE_IOT_HUB_DPS_REG_STATUS_NOT_STARTED");
	break;

	case AZURE_IOT_HUB_DPS_REG_STATUS_ASSIGNING:
		LOG_INF("DPS registration status: AZURE_IOT_HUB_DPS_REG_STATUS_ASSIGNING");
	break;

	case AZURE_IOT_HUB_DPS_REG_STATUS_ASSIGNED:
		LOG_INF("DPS registration status: AZURE_IOT_HUB_DPS_REG_STATUS_ASSIGNED");
		dps_was_successful = true;
		k_sem_give(&dps_done_sem);
	break;

	case AZURE_IOT_HUB_DPS_REG_STATUS_FAILED:
		LOG_INF("DPS registration status: AZURE_IOT_HUB_DPS_REG_STATUS_FAILED");
		dps_was_successful = false;
		k_sem_give(&dps_done_sem);
	break;

	default:
		LOG_WRN("Unhandled DPS status: %d", status);
	break;
	}
}

static int dps_run(struct azure_iot_hub_buf *hostname, struct azure_iot_hub_buf *device_id_in /*struct azure_iot_hub_buf *id_scope_in*/)
{
	int err;
	
	struct azure_iot_hub_dps_config dps_cfg = 
	{
		.handler = dps_handler,
		.reg_id = {
			.ptr = device_id_in->ptr,
			.size = strlen(device_id_in->ptr),
		},
		
		.id_scope = {
			.ptr = id_scope_buf,
			.size = strlen(id_scope_buf),
		},
	};
	
	
	LOG_INF("Starting DPS");

	err = azure_iot_hub_dps_init(&dps_cfg);
	if (err) 
	{
		LOG_ERR("azure_iot_hub_dps_init failed, error: %d", err);
		return err;
	}

	err = azure_iot_hub_dps_start();
    switch(err)
    {
        case 0:
            LOG_INF("The DPS process has started, timeout is set to %d seconds", CONFIG_AZURE_IOT_HUB_DPS_TIMEOUT_SEC);

            (void)k_sem_take(&dps_done_sem, K_FOREVER);

            if (!dps_was_successful) 
            {
                return -EFAULT;
            }
        break;

        case -EALREADY:
            LOG_INF("Already assigned to an IoT hub, skipping DPS");
        break;

        default:
            LOG_ERR("DPS failed to start, error: %d", err);
            return err;
        break;
    }

	err = azure_iot_hub_dps_hostname_get(hostname);
	if (err) 
	{
		LOG_ERR("Failed to get hostname, error: %d", err);
		return err;
	}

	err = azure_iot_hub_dps_device_id_get(device_id_in);
	if (err) 
	{
		LOG_ERR("Failed to get device ID, error: %d", err);
		return err;
	}

    LOG_INF("Device ID \"%s\" assigned to IoT hub with hostname \"%s\"", cfg.device_id.ptr, cfg.hostname.ptr);
	
    return 0;
}

//__________________________________________________________________________________
//Functions available outside module scope:
//Init azure manager module for connection 

//Consider if the best approach isn't to include a 'azure_iot_hub_config' struct from the main module, where it then comes from a external provisioning module...
//This would mean a simpler handling of provisioning since less "spaghetti" would be required
int AzureManagerInit(azureEventHandlerCb azureEventHandler, deviceTwinHandlerCb deviceTwinHandler, char *deviceIdStr, char *idScopeStr)
{
	LOG_INF("Azure init started!");
	int err;

	azureManagerHandler = azureEventHandler;
	dTHandler = deviceTwinHandler;

	//The device ide and id scope should come as strings, which then gets put into the cfg and dps_cfg structs 
	strcpy(device_id, deviceIdStr);
	strcpy(id_scope_buf, idScopeStr);

	cfg.device_id.size = strlen(device_id);
	cfg.hostname.size = strlen(hostname);

	struct azure_iot_hub_buf id_scope =
	{
		.ptr = id_scope_buf,
		.size = strlen(id_scope_buf),
	};

	LOG_INF("Device ID: \"%s\" - Size: [%d]", cfg.device_id.ptr, cfg.device_id.size);
	LOG_INF("ID Scope: \"%s\" - Size: [%d]", id_scope.ptr, id_scope.size);

	work_init();
	cJSON_Init();

	err = dps_run(&cfg.hostname, &cfg.device_id);
	if (err) 
	{
		LOG_ERR("Failed to run DPS, error: %d, terminating connection attempt", err);
		return -1;
	}


	err = azure_iot_hub_init(azure_event_handler);
	if (err) 
	{
		LOG_ERR("Azure IoT Hub could not be initialized, error: %d", err);
		return -1;
	}

	LOG_INF("Azure IoT Hub library initialized");


	return 0;
}

//Connect to azure
int AzureManagerConnect()
{
    int err;
	bool connected = false;

	//ToDo Add fallback counter in case connection isn't established
	while(connected == false)
	{
		err = azure_iot_hub_connect(&cfg);
		if(err == 0)
		{
			LOG_INF("Connection to Azure established");
			connected = true;
		} 
		else if(err == -EALREADY)
		{
			LOG_INF("Already connected to Azure IoT hub");
			connected = true;
		}
		k_sleep(K_MSEC(250)); //Wait for a short while before again trying to connect
	}

    return err;
}


//Disconnect from azure
int AzureManagerDisconnect()
{
    int err;

    err = azure_iot_hub_disconnect();
	if (err < 0) 
	{
		LOG_ERR("azure_iot_hub_disconnect failed: %d", err);
		return -1;
	}
	LOG_INF("Disconnected from Azure IoT hub");
    return 0;
}

//Send a telemtry message to azure. This needs to be a string, and can be formatted into a Json objet using cJson library
int AzureManagerSendTelemetry(char* telemetryString)
{
    int err;

//	static char buf[1024];
//	ssize_t len;
	struct azure_iot_hub_msg msg = 
	{
		.topic.type = AZURE_IOT_HUB_TOPIC_EVENT,
		.payload.ptr = telemetryString,
		.payload.size = strlen(telemetryString),
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
	};

/*
	len = snprintk(buf, sizeof(buf), telemetryString);

	if ((len < 0) || (len > sizeof(buf))) 
	{
		LOG_ERR("Failed to populate telemetry buffer");
        return -1;
	}

	msg.payload.size = len;
*/
	LOG_INF("Sending telemetry string: %s", telemetryString);

	err = azure_iot_hub_send(&msg);
	if (err) 
	{
		LOG_ERR("Failed to send telemetry");
		return -1;
	}

	LOG_INF("Telemetry was successfully sent");

	return 0;
}
//__________________________________________________________________________________

