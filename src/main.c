
//SDK includes
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <cJSON.h>
#include <date_time.h>
#include <net/azure_iot_hub.h>


//Custom module includes
//Azure connection modules
#include "azureConnection/azureManager.h"
#include "azureConnection/deviceReboot.h"
#include "azureConnection/deviceSettings.h"
#include "azureConnection/l4ConnectionManager.h"
#include "azureConnection/modemCommunicator.h"
#include "azureConnection/nrfProvisioningAzure.h"
#include "azureConnection/pam8053AzureDeviceTwin.h"

//External control modules
#include "externalControl/relayControl.h"

//RS485 modules
#include "rs485/codePanel.h"
#include "rs485/energyMeter.h"
#include "rs485/rs485Communication.h"

//Test modules
#include "test/pam8053SelfTest.h"

//Universal alarm input modules
#include "universalAlarmInput/adcDts.h"
#include "universalAlarmInput/universalAlarmInput.h"

//User interface modules
#include "userInterface/buttonModule.h"
#include "userInterface/indicatorModule.h"

//Zigbee modules
#include "zigbee/zigbeeManager.h"

//Global variables
int8_t L4ConnectionManagerStatusGlobal;
bool azureConnected = false;

Pam8053DeviceTwinStruct pam8053DtStruct;

uint32_t telemetryHeartbeat = 10; //This is equal to 10s, the heartbeat the first time the device connects to Azure after this time value
uint16_t heartbeatSendInterval = 300; //This is equal to 300s or 5min

//Buffers used to store values from provisioning module to put into azure manager module
char deviceId[16];
char idScope[12];
char serialNo[16];

char telemetryBuffer[1024];

typedef struct 
{
    char alarmId[64]; 			
    char name[64]; 				
    char text[64]; 				
    char eventTimestamp[32];
    uint8_t type; 				
    uint8_t priority;
} Alarm;

float dbMin = 0;
float dbMax = -100;

float dbmMin = 0;
float dbmMax = -100;

uint16_t timeToChangeProv = 0;

bool transmitHeartbeatTelemetry = false;
bool transmitEnergyMeterReading = false;

void heartbeatTimerHandlerCb(struct k_timer *timer) ;
K_TIMER_DEFINE(heartbeatTimer, heartbeatTimerHandlerCb, NULL); //This timer is used to send the heartbeat telemetry at the specified interval

void energyMeterTimerHandlerCb(struct k_timer *timer) ;
K_TIMER_DEFINE(energyMeterTimer, energyMeterTimerHandlerCb, NULL); //This timer is used to send the energy meter readings at the specified interval



LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);


//Function prototypes
void Pam80053AzureDeviceTwinCb();
void L4ConnectionManagerCb(const l4ConnectionManagerEvent* event);
void AzureManagerStatusCb(const azureManagerEvent* event);
void UniversalAlarmInputCb(const InputEvent* event);
void heartbeatTimerHandlerCb(struct k_timer *timer);

//Function prototypes for telemetry functions
void FormatTimestamp(char* pBuffer, size_t bufferSize, uint64_t timestamp);
cJSON *CreateConnectionDataObject();
cJSON* CreateAlarmObject(const Alarm* pAlarm, uint8_t alarmChannel);
cJSON* CreateHeartbeatTelemetry(void);
void TransmitHeartbeatTelemetry(void);

void updateTimer(struct k_timer *timer, uint32_t newInterval);





//Convience functions
//_____________________________________________________________________________________________________________________
void updateTimer(struct k_timer *timer, uint32_t newInterval)
{
	k_timer_stop(timer); //Stop the timer
	k_timer_start(timer, K_NO_WAIT, K_SECONDS(newInterval)); //Start the timer with the new interval
	LOG_INF("Timer updated to %ds", newInterval);
}


//_____________________________________________________________________________________________________________________
//Callback functions
void Pam80053AzureDeviceTwinCb()
{
	LOG_INF("AzureDeviceTwinCb called");

	updateTimer(&heartbeatTimer, pam8053DtStruct.heartbeatInterval); //Update the timer with the new interval
	updateTimer(&energyMeterTimer, pam8053DtStruct.powerMeterInterval);//Update the timer with the new interval

	//Update relays
	if(pam8053DtStruct.relay1Status == 1)
	{
		RelayControlRelayOn(1);
	}
	else
	{
		RelayControlRelayOff(1);
	}

	if(pam8053DtStruct.relay2Status == 1)
	{
		RelayControlRelayOn(2);
	}
	else
	{
		RelayControlRelayOff(2);
	}

	//Print the door code for debug purpose
	//ToDo Add communication and send this to code panel
	if(pam8053DtStruct.doorCode != NULL)
	{
		LOG_INF("Recived door code: %d",pam8053DtStruct.doorCode);
	}

	LOG_INF("New heartbeat interval has been set to %ds", heartbeatSendInterval);
}

void L4ConnectionManagerCb(const l4ConnectionManagerEvent* event)
{
	L4ConnectionManagerStatusGlobal = event->event;
	LOG_INF("networkStatusGlobal has value: %d",L4ConnectionManagerStatusGlobal);
}

void AzureManagerStatusCb(const azureManagerEvent* event)
{
	azureConnected = event->event == AZURE_MNG_NETWORK_CONNECTED ? true : false;;
	LOG_INF("azureConnected has value: %s", azureConnected ? "true" : "false");
}

void UniversalAlarmInputCb(const InputEvent* event)
{
	// In case of inputs, the alarm channel is the same as the input no.
	bool transmitTelemetry = false;

	switch (event->event)
	{
		case UIE_INIT_VALUE:
			LOG_INF("Init value on input %d", event->inputNo);

		break;

		case UIE_INPUT_CHANGED:
			LOG_INF("Input %d changed to %s", event->inputNo, event->value.bValue == true ? "Active" : "Inactive");

			transmitTelemetry = true;
		break;

		default:
			LOG_WRN("Unhandled input event type %d", event->event);
	}

	if (transmitTelemetry)
	{

	}
}

void CodePanelStatusCb(const codepanelEvent* event)
{
	//Handle the code panel status events here
	switch (event->status)
	{
		case CODE_PANEL_INIT:
			LOG_INF("Code panel initialized");
		break;

		case CODE_PANEL_CODE_ENTERED:
			LOG_INF("Code entered");
		break;

		case CODE_PANEL_CODE_ERROR:
			LOG_ERR("Code error: %d", event->errorCode);
			//ToDo add a report to the Azure device twin
			
		break;

		case DOOR_OPEN:
			LOG_INF("Door opened");
			//ToDo add a report to the Azure device twin
			IndicatorModuleLedBlinkFast(GREEN_LED); //Blink the green LED to indicate that the door was opened
		break;

		case DOOR_CLOSED:
			LOG_INF("Door closed");
			//ToDo add a report to the Azure device twin
			IndicatorModuleLedOff(GREEN_LED); //Turn of the green LED to indicate that the door was closed
		break;

		default:
			LOG_WRN("Unhandled code panel event type %d", event->status);
		break;
	}
}

void heartbeatTimerHandlerCb(struct k_timer *timer) 
{
    LOG_DBG("Timer expired!");
	transmitHeartbeatTelemetry = true;
	LOG_INF("Heartbeat telemetry sent, waiting for next interval of %ds", pam8053DtStruct.heartbeatInterval);
}

void energyMeterTimerHandlerCb(struct k_timer *timer) 
{
    LOG_DBG("Timer expired!");
	transmitEnergyMeterReading = true;
}

void buttonsHandlerCb(const buttonsHandlerEvent* status)
{

	LOG_INF("Button pressed: %d", status->buttonId);
	/*
	if (status->status == BUTTON_PRESSED) 
	{
		if (status->buttonId == 0) //Button 0 pressed
		{
			
			
		}
	}
	else if (status->status == BUTTON_RELEASED) 
	{

	}	
	*/
}

//Telemertry functions
//_____________________________________________________________________________________________________________________
void FormatTimestamp(char* pBuffer, size_t bufferSize, uint64_t timestamp)
{
	int err;
	time_t rawTime = timestamp / 1000; // milliseconds to seconds

	struct tm *time;
	time = gmtime(&rawTime);
	if (time == NULL) 
	{
    	LOG_ERR("Failed to convert Unix timestamp");
   }

	err = snprintf(pBuffer, bufferSize, "%04d-%02d-%02dT%02d:%02d:%02d",(time->tm_year + 1900),time->tm_mon+1,time->tm_mday,time->tm_hour,time->tm_min,time->tm_sec);
	if (err < 0)
	{
		LOG_ERR("Error in creating the eventTimestamp!");
	}
}

cJSON *CreateConnectionDataObject()
{
	int err;
	int8_t rxlev, ber, rscp, ecno, rsrq, rsrp;
	float rsrqLowerDb;
	int rsrpLowerDb;
	uint8_t band;

	cJSON *root = cJSON_CreateObject();

	err = ModemCommunicatorAtCommandCesq(&rxlev, &ber, &rscp, &ecno, &rsrq, &rsrp);
	if(err < 0)
	{
		LOG_ERR("Error in atCommandCesqReadAll, error: %d", err);
		LOG_ERR("Unable to add rsrq and rsrp values to telemetry data!");
	}
	else
	{
		//Add the rsrq and rsrp values to the heartbeat telemetry data, only the low values are calculated
		//since the high values simply are: rsrq_high = rsrq + 0.5 and rsrp = rsrp + 1
		cJSON_AddNumberToObject(root, "rsrq_low_dB", rsrqLowerDb = (rsrq - 40) / 2.0);
		cJSON_AddNumberToObject(root, "rsrp_low_dBm", rsrpLowerDb = rsrp - 141);
		
		if(rsrqLowerDb > dbMax)
		{
			dbMax = rsrqLowerDb;
		}
		else if(rsrqLowerDb < dbMin)
		{
			dbMin = rsrqLowerDb;
		}

		if(rsrpLowerDb > dbmMax)
		{
			dbmMax = rsrpLowerDb;
		}
		else if(rsrpLowerDb < dbmMin)
		{
			dbmMin = rsrpLowerDb;
		}
	}
	
	//Add the band to the telemetry data
	err = ModemCommunicatorAtCommandXcband(&band);
	if(err < 0)
	{
		LOG_ERR("Error in atCommandXCBand, error: %d", err);
		LOG_ERR("Unable to add band value to telemetry data!");
	}
	else
	{
		cJSON_AddNumberToObject(root, "band", band);
	}

	return root;
}

// Create a json object to send from an alarm, the user should make sure to delete the object after use
cJSON* CreateAlarmObject(const Alarm* pAlarm, uint8_t alarmChannel)
{
	cJSON *alarm = cJSON_CreateObject();

	// Create a single alarm object
	cJSON_AddStringToObject(alarm, "alarmId", pAlarm->alarmId);

	switch (alarmChannel)
	{
		//Alarm channel 1
		case 0:
			cJSON_AddStringToObject(alarm, "name", pam8053DtStruct.alarm0Name);
			cJSON_AddNumberToObject(alarm, "priority", pam8053DtStruct.alarm0Priority);
		break;

		//Alarm channel 2
		case 1:
			cJSON_AddStringToObject(alarm, "name", pam8053DtStruct.alarm1Name);
			cJSON_AddNumberToObject(alarm, "priority", pam8053DtStruct.alarm1Priority);
		break;

		//Alarm channel 3, this is the power failure alarm
		//For now is this text added in the CreatePowerFailureTelemetry function, but it could be added here as well
		case 3:

		break;

		//Alarm channel 4, this is the heartbeat alarm
		//For now is this text added in the CreateHeartbeatTelemetry function, but it could be added here as well
		case 4:

		break;

		//Can be used to implement a error alarm text, otherwise leave it empty
		default:	
			LOG_ERR("Alarm channel %d is not supported!", alarmChannel);
		break;
	}
	
	cJSON_AddStringToObject(alarm, "eventTimestamp", pAlarm->eventTimestamp);
	//Consider if this should be put into the switch statement instead, in case this should be device twin configurable
	cJSON_AddStringToObject(alarm, "text", pAlarm->text);
	cJSON_AddNumberToObject(alarm, "type", pAlarm->type);

	return alarm;
}

cJSON* CreateHeartbeatTelemetry(void)
{
	Alarm alarm;
	int64_t now = 0;
	int err;

	err = date_time_now_local(&now);

	snprintf(alarm.alarmId, sizeof(alarm.alarmId), "%s/HB", deviceId);
	alarm.type = 2;
	alarm.priority = 9;
	strncpy(alarm.name, deviceId, sizeof(alarm.name) - 1);
	strncpy(alarm.text, "Heartbeat", sizeof(alarm.text) - 1);
	FormatTimestamp(alarm.eventTimestamp, sizeof(alarm.eventTimestamp), now);

	//Create a json object to send
	cJSON *root = cJSON_CreateObject();

	//Create a connection data object and add it to the telemetry data
	cJSON *connectionDataObject = CreateConnectionDataObject();
	cJSON_AddItemToObject(root, "atCommandData", connectionDataObject);

	// The alarm object should be an array of alarm objects.
	cJSON *alarmsArray = cJSON_AddArrayToObject(root, "alarms");
	cJSON *object = CreateAlarmObject(&alarm, 4);
	cJSON_AddItemToArray(alarmsArray, object);
	return root;
}

void TransmitHeartbeatTelemetry(void)
{
	if (azureConnected)
	{
		//cJSON* pTelemetryObject = CreateFullAlarmTelemetry();
		cJSON* pTelemetryObject = CreateHeartbeatTelemetry();

		//"Export" the json object to the adcTelemetryBuffer
		cJSON_PrintPreallocated(pTelemetryObject, telemetryBuffer, sizeof(telemetryBuffer), false);
		
		cJSON_Delete(pTelemetryObject);
	
		//Send the telemetry data 
		AzureManagerSendTelemetry(telemetryBuffer);
	}
	else
	{
		LOG_INF("Azure is not connected, cannot send telemetry data!");
	}
}




void TransmitEnergyMeterTelemtry(void)
{
	if (azureConnected)
	{
		//cJSON* pTelemetryObject = CreateFullAlarmTelemetry();
		//cJSON* pTelemetryObject = CreateEnertyTelemetry();

		//"Export" the json object to the adcTelemetryBuffer
		//cJSON_PrintPreallocated(pTelemetryObject, telemetryBuffer, sizeof(telemetryBuffer), false);
		
		//cJSON_Delete(pTelemetryObject);
	
		//Send the telemetry data 
		//AzureManagerSendTelemetry(telemetryBuffer);
	}
	else
	{
		LOG_INF("Azure is not connected, cannot send telemetry data!");
	}
}

void setup()
{
	int err;

	
	//GPIO dependent modules initialization
		//Initialize the button module

		ButtonsHandlerInit(buttonsHandlerCb);
		ButtonsHandlerStart(GPIO_INT_EDGE_BOTH); //Start the button handler with edge both, this will trigger on both rising and falling edges

		//Initialize the indicator module
		IndicatorModuleInit(); //Initialize the indicator module, this will set the initial state of the indicators
		IndicatorModuleStart(); //Start the indicator module, this will enable the indicators


		

		//Initialize the module for universal alarm inputs
		UniversalAlarmInputInit(UniversalAlarmInputCb);
		UniversalAlarmInputSetMode(0, UIM_DIGITAL_INPUT_NC);
		UniversalAlarmInputSetMode(1, UIM_DIGITAL_INPUT_NC);
		UniversalAlarmInputStart();


		IndicatorModuleLedBlinkSlow(RED_LED); 


		//Initialize the relay control module

	//RS485 dependent modules initialization
		//Initialize the RS485 communication module

		//Initialize the code panel module

		//Initialize the energy meter module

	

	//Azure connection modules initialization
		//Run provisioning check
		err = NrfProvisioningAzureRunProvisioningCheck(deviceId, idScope, serialNo, timeToChangeProv);
		if (err < 0)
		{
			LOG_ERR("Provisioning check failed, rebooting device");
			DeviceRebootError(); //Consider if this always is the best option...
		}

		//Initialize the device settings module
	
		//Initialize the device twin module for PAM8053
		Pam8053AzureDeviceTwinSetup(&pam8053DtStruct, Pam80053AzureDeviceTwinCb);

		//initialize l4 connection manager
		err = L4ConnectionManagerNetworkInit(L4ConnectionManagerCb);
		if (err < 0)
		{
			LOG_ERR("L4 connection manager initialization failed, rebooting device");
			DeviceRebootError();
		}

		//Connect to the network
		err = L4ConnectionManagerNetworkConnect(300);
		if (err < 0)
		{
			LOG_ERR("L4 connection manager network connect failed, rebooting device");
			DeviceRebootError();
		}
		LOG_INF("L4 connection manager network connect successful");

		//Initialize the Azure connection manager
		err = AzureManagerInit(AzureManagerStatusCb, Pam8053DeviceTwinCb, "PAM8002_1040", "0ne008A3851");//deviceId, idScope);
		if (err < 0)
		{
			LOG_ERR("Azure manager initialization failed, rebooting device");
			DeviceRebootError();
		}
		LOG_INF("Azure manager initialization successful");
	
	//Zigbee dependent modules initialization
	//Initialize the Zigbee manager
	

	//Setup of the different modules

	k_timer_start(&heartbeatTimer, K_SECONDS(heartbeatSendInterval), K_SECONDS(heartbeatSendInterval)); //Start the heartbeat timer with the initial interval
	k_timer_start(&energyMeterTimer, K_SECONDS(pam8053DtStruct.powerMeterInterval), K_SECONDS(pam8053DtStruct.powerMeterInterval)); //Start the energy meter timer with the initial interval

}




//Main function
//_____________________________________________________________________________________________________________________
int main(void)
{
	int err;
	LOG_INF("Starting PAM8053 device, firmware version: %s", CONFIG_AZURE_FOTA_APP_VERSION);

	setup(); //Setup the modules
	k_sleep(K_MSEC(1000)); //Wait for the modules to initialize

		
	while(1)
	{

		if (!azureConnected && L4ConnectionManagerStatusGlobal == L4_CNCT_MNG_NETWORK_CONNECTED)
		{
			LOG_INF("Connecting to Azure...");
			err = AzureManagerConnect();
			if (err < 0 && err != -EALREADY) 
			{
				LOG_ERR("Could not connect to Azure ");
				DeviceRebootError();
			}
		}
		
		if (transmitHeartbeatTelemetry)
		{
			TransmitHeartbeatTelemetry();
			LOG_INF("Heartbeat telemetry sent, waiting for next interval of %ds", pam8053DtStruct.heartbeatInterval);
			transmitHeartbeatTelemetry = false; //Reset the flag
		}



		if (transmitEnergyMeterReading)
		{
			TransmitEnergyMeterTelemtry();
			LOG_INF("Energy meter reading sent, waiting for next interval of %ds", pam8053DtStruct.powerMeterInterval);
			transmitEnergyMeterReading = false; //Reset the flag
		}

		if(ZigbeeManagerGetTemp(1) < 18.0)//If the temperature is below 18 degrees Celsius, turn on the heating
		{
			LOG_INF("Temperature is below 18 degrees Celsius, turning on the heating");
			RelayControlRelayOn(1); //Turn on the heating relay
		} 
		else if(ZigbeeManagerGetTemp(1) > 22.0) //If the temperature is above 22 degrees Celsius, turn off the heating
		{
			LOG_INF("Temperature is above 22 degrees Celsius, turning off the heating");
			RelayControlRelayOff(1); //Turn off the heating relay
		}


		k_sleep(K_MSEC(1000));	
	}
		
	return 0;
}
