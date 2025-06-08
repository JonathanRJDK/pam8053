
#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>

//Used to call modem functions
#include <modem/nrf_modem_lib.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>

//Used to communicate with UART
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>

#include <net/azure_iot_hub.h>
#include <net/azure_iot_hub_dps.h>

#include <zephyr/sys/reboot.h>

#include "nrfProvisioningAzure.h"
#include "deviceReboot.h"
#include "deviceSettings.h"

LOG_MODULE_REGISTER(nrfProvisioningAzure, LOG_LEVEL_DBG);

//TODO consider using dynamic memory allocation to minimize RAM impact, since the buffers in this module both should and is cleared at the end for security reasons
//The dynamic memory should just use the simple C functions for memory al-and delocation. (malloc and free)
char txBuf[MAX_BUFFER_LENGTH];                  //TODO Figure out why this NEEDS to be declared globally. Program crashes if not?!

uint8_t credentialsBuffer[MAX_BUFFER_LENGTH]; 	//TODO Figure out why this NEEDS to be declared globally. Program crashes if not?!
uint8_t txMenuResponseBuf[MAX_BUFFER_LENGTH];					//TODO Figure out why this NEEDS to be declared globally. Program crashes if not?!

const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));


//Receive buffer used in UART ISR callback
static char rxBuf[MAX_BUFFER_LENGTH];
static int rxBufPos;

//Queue to store up to 5 messages (aligned to 4-byte boundary) (this might be a bit overkill for this specific application)
K_MSGQ_DEFINE(uartMsgq, MAX_BUFFER_LENGTH, 5, 4);

//function prototypes
int atCommandCmngRead(int securityTag, int type);
void atCommandCmngWrite(int securityTag, int type, const char* provisionDataBuf);

//Read characters from UART until line end is detected. Afterwards push the data to the message queue.
static void serialCb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart)) {
		return;
	}

	if (!uart_irq_rx_ready(uart)) {
		return;
	}

	//Read until FIFO reaches newline or carriage return
	while (uart_fifo_read(uart, &c, 1) == 1) 
	{
		if ((c == '\n' || c == '\r') && rxBufPos > 0) 
		{
			//Terminate the string with a null character
			rxBuf[rxBufPos] = '\0'; 

			//If queue is full, message is silently dropped
			k_msgq_put(&uartMsgq, &rxBuf, K_NO_WAIT);

			//Reset the buffer (it was copied to the msgq)
			rxBufPos = 0;
		}
		else if (rxBufPos < (sizeof(rxBuf) - 1) && (c != '\n' && c != '\r')) 
			//If the buffer is not full, add the character to the buffer
			//If the buffer is full, the character is dropped
		{
			rxBuf[rxBufPos++] = c;
		}
		//Else: characters beyond buffer size are dropped 
	}
}

//To strictly comply with UART timing, enable external XTAL oscillator
static void enableXtal(void)
{
	struct onoff_manager *clk_mgr;
	static struct onoff_client cli = {};

	clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	sys_notify_init_spinwait(&cli.notify);
	(void)onoff_request(clk_mgr, &cli);
}

//Clears the input buffer, used to ensure private data doesn't remain in a buffer somewhere in memory
void clearBuffer(uint8_t* bufferToBeCleared)
{
	strcpy(bufferToBeCleared,"");
}

//Function that prompts the user for the data to be provisioned
static int askForCredentials(uint8_t* outputBuffer, uint16_t lengthOfCredentials, k_timeout_t messageTimeout)
{
	//Wait for input from the user for the specified amount of time, use K_FOREVER if you wanna wait in the k_msgq_get function forever
	//Otherwise use functions such as K_MSEC(10); as input in the messageTimeout parameter
	uint8_t state = 0;
	int result;

	LOG_DBG("Expecting a credential of max length: %d",lengthOfCredentials);

	while(1)
	{
	switch (state)
		{
			case 0:
				result = k_msgq_get(&uartMsgq, &txBuf, messageTimeout);
				if (result == 0)
				{
					if(strlen(txBuf)>lengthOfCredentials)
					{
						LOG_ERR("ERROR: Length of input data exceeds expectations!");
						return -1;
					}
					k_sleep(K_MSEC(10));

					LOG_DBG("\nTo accept send 'Y' to enter another send 'N', to exit without saving send 'E'");
					state = 1;
				}
				else
				{
					LOG_ERR("No input detected");
					return -1;
				}
			break;

			case 1:
				result = k_msgq_get(&uartMsgq, &txMenuResponseBuf, messageTimeout);
				if (result == 0)
				{
					if(txMenuResponseBuf[0]=='Y')
					{
						LOG_DBG("Credentials have been stored!");
						strcpy(outputBuffer,txBuf);
						clearBuffer(txBuf);
						return 0;
					} 
					else if(txMenuResponseBuf[0]=='N') 
					{
						state = 0;
					} 
					else if(txMenuResponseBuf[0]=='E') 
					{
						LOG_ERR("Credentials have not been stored");
						clearBuffer(txBuf);
						return 1;
					}
					else if(txMenuResponseBuf[0]=='D')
					{
						LOG_DBG("D option selected, no action taken");
					}
					else
					{
						LOG_ERR("Unknown entered\n");
						clearBuffer(txBuf);
					}
				}
				else
				{
					LOG_ERR("No input detected\n");
					return -1;
				}
			break;

			default:
				return -1;
			break;
		}
	}
	return 0;
}

//Function that asks the user if the credentials already saved should be changed
//Will return 1 if the user wants to change the credentials, 0 if not and -1 if an error occurred
static int checkForCharInInputBuffer(char charToCheckFor ,k_timeout_t messageTimeout)
{
	int result;

	LOG_DBG("Credential already found!\nSend a '%c' to change this to something else", charToCheckFor);
	LOG_DBG("Send a 'N' to keep the current credentials");

	result = k_msgq_get(&uartMsgq, &txBuf, messageTimeout);
	if (result == 0)
	{
		if(strlen(txBuf) > 1)
		{
			LOG_ERR("ERROR: Length of input data exceeds expectations! Only 1 character expected!");
			result = -1;
		}

		if(txBuf[0] == charToCheckFor)
		{
			result = 1;
		}

		if(txBuf[0] == 'N')
		{
			result = 0;
		}
	}
	else
	{
		LOG_ERR("No input detected");
		result = 0;
	}
	return result;
}

static void decodePemFiles(char* inputPemBuf) {
    typedef enum {
        HEADER = 0,
        BODY = 1,
        FOOTER = 2
    } decodeStates;

    decodeStates decodeState = HEADER;
    uint16_t index = 0;
    uint8_t dashCounter = 0;
    bool isFinished = false;

    char atCommandBuffer[strlen(inputPemBuf)]; // Buffer for modified output

    while (!isFinished) {
        switch (decodeState) {
            case HEADER:
                atCommandBuffer[index] = inputPemBuf[index];

                if (inputPemBuf[index] == '-') 
				{
                    dashCounter++;
                }

                if (dashCounter == 10) // Header completed
				{ 
                    dashCounter = 0;
                    decodeState = BODY;
					break;
                }
                index++;
                break;

            case BODY:
                while (inputPemBuf[index] != '\0') 
				{
                    if (inputPemBuf[index] == ' ') 
					{
                        atCommandBuffer[index] = '\n'; // Replace spaces with new lines
                    } 
					else 
					{
                        atCommandBuffer[index] = inputPemBuf[index];
                    }

                    index++;

                    if (inputPemBuf[index] == '-') // Footer detected
					{ 
                        decodeState = FOOTER;
                        break;
                    }
                }
                break;

            case FOOTER:
                atCommandBuffer[index] = inputPemBuf[index];

                if (inputPemBuf[index] == '-') {
                    dashCounter++;
                }

                if (dashCounter == 10) // Footer completed
				{ 
                    isFinished = true;
                }

                index++;
                break;
        }
    }

	// Add null terminator to the end of the modified string
    atCommandBuffer[strlen(atCommandBuffer)] = '\0';
    strcpy(inputPemBuf, atCommandBuffer);
}


//Function that sends an AT command which writes provisioning data, gets the response, checks for errors and prints the response from the modem.
//-securityTag is the security tag of the data
//-type is the type of data to be saved: certificate, key
//-provisionDataBuf is a pointer to the buffer containing data to be provisioned
void atCommandCmngWrite(int securityTag, int type, const char* provisionDataBuf)
{
	LOG_DBG("%s\n",provisionDataBuf);
	int err;
	char atResponseBuf[255];

	LOG_DBG("Write function - Security tag %d Type: %d", securityTag, type); //TODO Might need a check that values fall within acceptable range?

	err = nrf_modem_at_cmd(atResponseBuf,sizeof(atResponseBuf),"AT%%CMNG=%d,%d,%d,\"%s\"",CMNG_COMMAND_WRITE, securityTag, type, provisionDataBuf);
	if(err!=0)
	{
		LOG_ERR("ERR at sending at commands, code: %d",err);
	} 
	LOG_DBG("Modem response: \n%s\n",atResponseBuf);
	atCommandCmngRead(securityTag, type);
}

//Function that sends an AT command which reads provisioning data, gets the response, checks for errors and prints the response from the modem.
//Returns 1 upon data already being at a security tag, return 0 if not
//-securityTag is the security tag of the data
//-type is the type of data to be saved: certificate, key
int atCommandCmngRead(int securityTag, int type)
{
	int err;
	char atResponseBuf[255];

	LOG_DBG("Read function - code:Security tag %d Type: %d", securityTag, type); //TODO Might need a check that values fall within acceptable range?

	err = nrf_modem_at_cmd(atResponseBuf,sizeof(atResponseBuf),"AT%%CMNG=%d,%d,%d",CMNG_COMMAND_READ, securityTag, type);
	if(err!=0)
	{
		LOG_DBG("ERR at sending at commands, code: %d",err);
	} 

	LOG_DBG("Modem response: \n%s\n",atResponseBuf);	

	if(atResponseBuf[81] == 'O' && atResponseBuf[82] == 'K')
	{
		return 1;
	}
	return 0;
}

//Function that sends an AT command which deletes provisioning data, gets the response, checks for errors and prints the response from the modem.
//-securityTag is the security tag of the data
//-type is the type of data to be saved: certificate, key
void atCommandCmngDelete(int securityTag, int type)
{
	int err;
	char atResponseBuf[255];

	LOG_DBG("Delete function - Security tag %d Type: %d", securityTag, type); //TODO Might need a check that values fall within acceptable range?

	err = nrf_modem_at_cmd(atResponseBuf,sizeof(atResponseBuf),"AT%%CMNG=%d,%d,%d",CMNG_COMMAND_DELETE, securityTag, type);
	if(err!=0)
	{
		LOG_DBG("ERR at sending at commands, code: %d",err);
	} 
	LOG_DBG("Modem response: \n%s\n",atResponseBuf);	
}

int atCommandCmngReadAllHash()
{
	int err;
	char atResponseBuf[255];

	LOG_DBG("Read function ");

	err = nrf_modem_at_cmd(atResponseBuf,sizeof(atResponseBuf),"AT%%CMNG=1");
	if(err!=0)
	{
		LOG_DBG("ERR at sending at commands, code: %d",err);
	} 

	LOG_DBG("Modem response:\n%s\n",atResponseBuf);	
	if(atResponseBuf[0] == 'O' && atResponseBuf[1] == 'K')
	{
		return 1;
	}
	return 0;
}

void DeviceSettingsHandler(DeviceSettingsStatus status)
{

}

//Main function of the module which checks for already provisioned data, skips if it exits otherwise prompts the different data that needs to be provisioned
//in order for an application to connect to Microsoft Azure IoT hub using a SoftSim from ONOMONDO. 
//Returns a -1 upon error and 0 upon success
int NrfProvisioningAzureRunProvisioningCheck(char *deviceIdBuf, char *idScopeBuf, char *serialNoBuf, uint16_t timeToChangeProvData)
{
	int err;
	int result;

	bool isProvisioningFinished = 0;
	enum 
	{
		idle = 0,

		checkForDeviceId = 1,
		askForDeviceId = 2,

		checkForSerialNo = 3,
		askForSerialNo = 4,

		checkForIdScope = 5,
		askForIdScope = 6,

		checkForCAMainCert = 7,
		askForCAMainCert = 8,

		checkForCASecCert = 9,
		askForCASecCert = 10,

		checkForClientCert = 11,
		askForClientCert = 12,

		checkForPrivateKey = 13,
		askForPrivateKey = 14,

		provisioningFinished = 15,
	} provisioningState;

	DeviceSettingsBuffer bufferDeviceId;
	DeviceSettingsBuffer bufferSerialNo;
	DeviceSettingsBuffer bufferIdScope;

	DeviceSettings settingsCfg =
	{
		.handler = DeviceSettingsHandler
	};

	LOG_INF("runProvisioningCheck started");

	DeviceSettingsInit(&settingsCfg);
	if (DeviceSettingsStart() != 0)
	{
		LOG_ERR("Failed to start device settings");
		DeviceRebootError();
	}

	if (!device_is_ready(uart)) 
	{
    	return -1;
	}

	uart_irq_callback_user_data_set(uart, serialCb, NULL);
	uart_irq_rx_enable(uart);
	enableXtal();

	err = nrf_modem_lib_init();
	if (err) 
	{
		printk("Modem library initialization failed, error: %d", err);
		return -1;
	}

	provisioningState = idle;

	//Main switch statement that functions as the "menu" for provisioning
	while(isProvisioningFinished!=1)
	{
		switch (provisioningState)
		{
			case idle:
				provisioningState = checkForDeviceId;
			break;	
	

			case checkForDeviceId:
				LOG_DBG("Checking for already provisioned device id...");
				
				err = DeviceSettingsGetDeviceId(&bufferDeviceId);
				//Setting was found
				if(err == 0) 
				{
					LOG_DBG("Found device id: %s", bufferDeviceId.ptr);
					LOG_DBG("Send a 'C' within %d seconds to change this to something else",timeToChangeProvData);
					//Check if the user wants to change the device id
					result = checkForCharInInputBuffer('C', K_SECONDS(timeToChangeProvData));
					if(result == 1)
					{
						LOG_DBG("Changing device id to something else!");
						provisioningState = askForDeviceId;
					}
					else if(result == 0)
					{
						LOG_DBG("Keeping the device id!");
						//Load the device id into the buffer to be used in the dps
						strcpy(deviceIdBuf, bufferDeviceId.ptr);
						provisioningState = checkForIdScope;
					}
					else
					{
						LOG_ERR("Unknown command, keeping the device id!");
						//Load the device id into the buffer to be used in the dps
						strcpy(deviceIdBuf, bufferDeviceId.ptr);
						provisioningState = checkForIdScope;
					}
				}
				//No setting was found
				else if(err == -ENOENT) 
				{
					LOG_DBG("No device id found!");
					provisioningState = askForDeviceId;
				}
				else 
				{
					LOG_ERR("Error in getting the device id, code: %d", err);
					provisioningState = checkForIdScope;
				}
			break;

			case askForDeviceId:
				LOG_DBG("Please provide a device id:");
				err = askForCredentials(credentialsBuffer, DEVICE_ID_MAX_SIZE,K_MINUTES(1));
				if(err == 0)
				{
					//Save the device id to the NVS partition
					DeviceSettingsSaveDeviceId(credentialsBuffer, 16);
					//Load the device id into the buffer to be used in the dps
					strcpy(deviceIdBuf, credentialsBuffer);
					LOG_DBG("Device id stored: %s", credentialsBuffer);
					clearBuffer(credentialsBuffer);
				}
				provisioningState = checkForIdScope;
			break;


			case checkForIdScope:
				LOG_DBG("Checking for already provisioned id scope...");
				err = DeviceSettingsGetScopeId(&bufferIdScope);
				//Setting was found
				if(err == 0) 
				{
					LOG_DBG("Found id scope: %s", bufferIdScope.ptr);
					LOG_DBG("Send a 'C' within %d seconds to change this to something else", timeToChangeProvData);
					//Check if the user wants to change the device id
					result = checkForCharInInputBuffer('C', K_SECONDS(timeToChangeProvData));
					if(result == 1)
					{
						LOG_DBG("Changing id scope to something else!");
						provisioningState = askForIdScope;
					}
					else if(result == 0)
					{
						LOG_DBG("Keeping the id scope!");
						//Load the device id into the buffer to be used in the dps
						strcpy(idScopeBuf, bufferIdScope.ptr);
						provisioningState = checkForSerialNo;
					}
					else
					{
						LOG_ERR("Unknown command, keeping the id scope!");
						//Load the device id into the buffer to be used in the dps
						strcpy(idScopeBuf, bufferIdScope.ptr);
						provisioningState = checkForSerialNo;
					}
				}
				//No setting was found
				else if(err == -ENOENT) 
				{
					LOG_DBG("No id scope found!");
					provisioningState = askForIdScope;
				}
				else 
				{
					LOG_ERR("Error in getting the id scope, code: %d", err);
					provisioningState = checkForSerialNo;
				}
			break;

			case askForIdScope:
				LOG_DBG("Please provide a id scope:");
				err = askForCredentials(credentialsBuffer, ID_SCOPE_MAX_SIZE,K_MINUTES(1));
				if(err == 0)
				{
					//Save the device id to the NVS partition
					DeviceSettingsSaveScopeId(credentialsBuffer, 12);
					//Load the device id into the buffer to be used in the dps
					strcpy(idScopeBuf, credentialsBuffer);
					LOG_DBG("Id scope stored: %s", credentialsBuffer);
					clearBuffer(credentialsBuffer);
				}
				provisioningState = checkForSerialNo;
			break;


			case checkForSerialNo:
				LOG_DBG("Checking for already provisioned serial number...");
				err = DeviceSettingsGetSerialNo(&bufferSerialNo);
				//Setting was found
				if(err == 0) 
				{
					LOG_DBG("Found serial number: %s", bufferSerialNo.ptr);
					LOG_DBG("Send a 'C' within %d seconds to change this to something else", timeToChangeProvData);
					//Check if the user wants to change the device id
					result = checkForCharInInputBuffer('C', K_SECONDS(timeToChangeProvData));
					if(result == 1)
					{
						LOG_DBG("Changing serial number to something else!");
						provisioningState = askForSerialNo;
					}
					else if(result == 0)
					{
						LOG_DBG("Keeping the serial number!");
						//Load the device id into the buffer to be used in the dps
						strcpy(serialNoBuf, bufferSerialNo.ptr);
						provisioningState = checkForCAMainCert;
					}
					else
					{
						LOG_ERR("Unknown command, keeping the serial number!");
						//Load the device id into the buffer to be used in the dps
						strcpy(serialNoBuf, bufferSerialNo.ptr);
						provisioningState = checkForCAMainCert;
					}
				}
				//No setting was found
				else if(err == -ENOENT) 
				{
					LOG_DBG("No serial number found!");
					provisioningState = askForSerialNo;
				}
				else 
				{
					LOG_ERR("Error in getting the serial number, code: %d", err);
					provisioningState = checkForCAMainCert;
				}
			break;

			case askForSerialNo:
				LOG_DBG("Please provide a serial number:");
				err = askForCredentials(credentialsBuffer, SERIAL_NO_MAX_SIZE,K_MINUTES(1));
				if(err == 0)
				{
					//Save the device id to the NVS partition
					DeviceSettingsSaveSerialNo(credentialsBuffer, 16);
					//Load the device id into the buffer to be used in the dps
					strcpy(serialNoBuf, credentialsBuffer);
					LOG_DBG("Serial number stored: %s", credentialsBuffer);
					clearBuffer(credentialsBuffer);
				}
				provisioningState = checkForCAMainCert;
			break;


			case checkForCAMainCert:
				LOG_DBG("Checking for already provisioned main CA certificate...");
				err = atCommandCmngRead(SEC_TAG_MAIN, CMNG_TYPE_CA_CERT);
				//Certificate was found
				if(err == 1) 
				{
					LOG_DBG("Found main CA certificate!");
					LOG_DBG("Send a 'C' within %d seconds to change this to something else", timeToChangeProvData);
					result = checkForCharInInputBuffer('C', K_SECONDS(timeToChangeProvData));
					if(result == 1)
					{
						LOG_DBG("Changing main CA certificate to something else!");
						provisioningState = askForCAMainCert;
					}
					else if(result == 0)
					{
						LOG_DBG("Keeping the main CA certificate!");
						provisioningState = checkForCASecCert;
					}
					else
					{
						LOG_ERR("Unknown command, keeping the main CA certificate!");
					}
				}

				//No certificate was found
				else if(err == 0) 
				{
					LOG_DBG("No main CA certificate found!");
					provisioningState = askForCAMainCert;
				}
				else 
				{
					LOG_ERR("Error in getting the main CA certificate, code: %d", err);
					provisioningState = checkForCASecCert;
				}
			break;

			case askForCAMainCert:
				LOG_DBG("No main CA certificate found!\nSend the main CA certificate:");
				err = askForCredentials(credentialsBuffer, CA_CERT_MAIN_SIZE,K_MINUTES(1));
				if(err == 0)
				{
					LOG_DBG("Main CA certificate stored: %s", credentialsBuffer);
					decodePemFiles(credentialsBuffer);
					atCommandCmngWrite(SEC_TAG_MAIN,CMNG_TYPE_CA_CERT,credentialsBuffer);
					clearBuffer(credentialsBuffer);
				}
				else
				{
					LOG_ERR("Error in storing the main CA certificate, code: %d", err);
				}
				provisioningState = checkForCASecCert;

			break;


			case checkForCASecCert:
				LOG_DBG("Checking for already provisioned secondary CA certificate...");
				err = atCommandCmngRead(SEC_TAG_SECONDARY,CMNG_TYPE_CA_CERT);
				//Certificate was found
				if(err == 1) 
				{
					LOG_DBG("Found secondary CA certificate!");
					LOG_DBG("Send a 'C' within %d seconds to change this to something else", timeToChangeProvData);
					result = checkForCharInInputBuffer('C', K_SECONDS(timeToChangeProvData));
					if(result == 1)
					{
						LOG_DBG("Changing secondary CA certificate to something else!");
						provisioningState = askForCASecCert;
					}
					else if(result == 0)
					{
						LOG_DBG("Keeping the secondary CA certificate!");
						provisioningState = checkForClientCert;
					}
					else
					{
						LOG_ERR("Unknown command, keeping the secondary CA certificate!");
					}
				}
				//No certificate was found
				else if(err == 0) 
				{
					LOG_DBG("No secondary CA certificate found!");
					provisioningState = askForCASecCert;
				}
				else 
				{
					LOG_ERR("Error in getting the secondary CA certificate, code: %d", err);
					provisioningState = checkForClientCert;
				}
			break;

			case askForCASecCert:
				LOG_DBG("No secondary CA certificate found!\nSend the secondary CA certificate:");
				err = askForCredentials(credentialsBuffer, CA_CERT_SECONDARY_SIZE,K_MINUTES(1));
				if(err == 0)
				{
					LOG_DBG("Secondary CA certificate stored: %s", credentialsBuffer);
					decodePemFiles(credentialsBuffer);
					atCommandCmngWrite(SEC_TAG_SECONDARY,CMNG_TYPE_CA_CERT,credentialsBuffer);
					clearBuffer(credentialsBuffer);
				}
				else
				{
					LOG_ERR("Error in storing the secondary CA certificate, code: %d", err);
				}
				provisioningState = checkForClientCert;
			break;


			case checkForClientCert:
				LOG_DBG("Checking for already provisioned client certificate...");
				err = atCommandCmngRead(SEC_TAG_MAIN,CMNG_TYPE_CLIENT_CERT);
				//Certificate was found
				if(err == 1) 
				{
					LOG_DBG("Found client certificate!");
					LOG_DBG("Send a 'C' within %d seconds to change this to something else", timeToChangeProvData);
					result = checkForCharInInputBuffer('C', K_SECONDS(timeToChangeProvData));
					if(result == 1)
					{
						LOG_DBG("Changing client certificate to something else!");
						provisioningState = askForClientCert;
					}
					else if(result == 0)
					{
						LOG_DBG("Keeping the client certificate!");
						provisioningState = checkForPrivateKey;
					}
					else
					{
						LOG_ERR("Unknown command, keeping the client certificate!");
					}
				}
				//No certificate was found
				else if(err == 0) 
				{
					LOG_DBG("No client certificate found!");
					provisioningState = askForClientCert;
				}
				else 
				{
					LOG_ERR("Error in getting the client certificate, code: %d", err);
					provisioningState = checkForPrivateKey;
				}
			break;

			case askForClientCert:
				LOG_DBG("No client certificate found!\nSend the client certificate:");
				err = askForCredentials(credentialsBuffer, CLIENT_CERT_SIZE,K_MINUTES(1));
				if(err == 0)
				{
					LOG_DBG("Client certificate stored: %s", credentialsBuffer);
					decodePemFiles(credentialsBuffer);
					atCommandCmngWrite(SEC_TAG_MAIN,CMNG_TYPE_CLIENT_CERT,credentialsBuffer);
					clearBuffer(credentialsBuffer);
				}
				else
				{
					LOG_ERR("Error in storing the client certificate, code: %d", err);
				}
				provisioningState = checkForPrivateKey;
			break;


			case checkForPrivateKey:
				LOG_DBG("Checking for already provisioned private key...");
				err = atCommandCmngRead(SEC_TAG_MAIN,CMNG_TYPE_PRIVATE_KEY);
				//Certificate was found
				if(err == 1) 
				{
					LOG_DBG("Found private key!");
					LOG_DBG("Send a 'C' within %d seconds to change this to something else", timeToChangeProvData);
					result = checkForCharInInputBuffer('C', K_SECONDS(timeToChangeProvData));
					if(result == 1)
					{
						LOG_DBG("Changing private key to something else!");
						provisioningState = askForPrivateKey;
					}
					else if(result == 0)
					{
						LOG_DBG("Keeping the private key!");
						provisioningState = provisioningFinished;
					}
					else
					{
						LOG_ERR("Unknown command, keeping the private key!");
					}
				}
				//No certificate was found
				else if(err == 0) 
				{
					LOG_DBG("No private key found!");
					provisioningState = askForPrivateKey;
				}
				else 
				{
					LOG_ERR("Error in getting the private key, code: %d", err);
					provisioningState = provisioningFinished;
				}
			break;

			case askForPrivateKey:
				LOG_DBG("No private key found!\nSend the private key:");
				err = askForCredentials(credentialsBuffer, PRIVATE_KEY_SIZE,K_MINUTES(1));
				if(err == 0)
				{
					LOG_DBG("Private key stored: %s", credentialsBuffer);
					decodePemFiles(credentialsBuffer);
					atCommandCmngWrite(SEC_TAG_MAIN,CMNG_TYPE_PRIVATE_KEY,credentialsBuffer);
					clearBuffer(credentialsBuffer);
				}
				else
				{
					LOG_ERR("Error in storing the private key, code: %d", err);
				}
				provisioningState = provisioningFinished;
			break;

			case provisioningFinished:
				LOG_DBG("Provisioning finished!\n");
				LOG_DBG("Device id: %s", deviceIdBuf);
				LOG_DBG("Id scope: %s", idScopeBuf);
				LOG_DBG("Serial number: %s", serialNoBuf);
				isProvisioningFinished = 1;
			break;
		}
	}
    uart_irq_rx_disable(uart);
	return 0;
}

//Function that prompts the user for the data to be provisioned
int deleteAzureProvisioning(k_timeout_t messageTimeout)
{
	LOG_DBG("Deleting already provisioned Azure data!\n");
	atCommandCmngDelete(SEC_TAG_MAIN,CMNG_TYPE_CA_CERT);
	atCommandCmngDelete(SEC_TAG_SECONDARY,CMNG_TYPE_CA_CERT);
	atCommandCmngDelete(SEC_TAG_MAIN,CMNG_TYPE_CLIENT_CERT);
	atCommandCmngDelete(SEC_TAG_MAIN,CMNG_TYPE_PRIVATE_KEY);
	return 0;
}


int readStoredProvisioning()
{
	atCommandCmngRead(SEC_TAG_MAIN,0);
	atCommandCmngRead(SEC_TAG_MAIN,1);
	atCommandCmngRead(SEC_TAG_MAIN,2);
	atCommandCmngRead(SEC_TAG_SECONDARY,0);
	return 0;
}