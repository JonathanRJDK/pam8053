#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>

#include <modem/nrf_modem_lib.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(modemCommunicator, LOG_LEVEL_ERR);

int ModemCommunicatorInit()
{
    int err;
    
    err = nrf_modem_lib_init();
	if (err) 
	{
		LOG_ERR("Modem library initialization failed, error: %d", err);
        if(err == -1)
        {
            LOG_ERR("Modem library already initialized, error: %d", err);
        }
		return -err;
	}
    return err;
}

//Function that gets the ban being used by the modem, this is done using the AT command XCBAND
int ModemCommunicatorAtCommandXcband(uint8_t *band)
{
    LOG_INF("Reading band information using AT command XCBAND");
    int err;
    char responseBuffer[100];

    // Send the AT command to read the band information
    err = nrf_modem_at_cmd(responseBuffer, sizeof(responseBuffer), "AT%%XCBAND");
    if (err) 
    {
        LOG_ERR("Error sending AT command: %d\n", err);
        return -1;
    }

    //ToDo fix this response check, it is not correct and should be fixed to check for the correct response format
    //However it does seem to be correct..
    // Validate the response format
    /*
    if (strstr(responseBuffer, "%%XCBAND:") == NULL) 
    {
        LOG_ERR("Unexpected response format:[%s]\n", responseBuffer);
        return -2; // Indicate a response format error
    }
    */

    // Parse the response to extract the band value
    int parsedValues = sscanf(responseBuffer, "%*[^:]: %hhu", band);
    if (parsedValues != 1) 
    {
        LOG_ERR("Error parsing response: %s\n", responseBuffer);
        return -3; // Indicate a parsing error
    }

    // Check if the band value is valid (1-85)
    if (*band >= 85 && *band <= 0) 
    {
        LOG_ERR("Invalid band value: %u\n", *band);
        return -4; // Indicate an invalid band value
    }

    return 0; // Success
}

//Function that gets the signal quality and strength using the AT command CESQ
int ModemCommunicatorAtCommandCesq(uint8_t *rxlev, uint8_t *ber, uint8_t *rscp, uint8_t *ecno, uint8_t *rsrq, uint8_t *rsrp)
{
    LOG_INF("Reading signal quality using AT command CESQ");
    int err;
    char responseBuffer[100];

    // Send the AT command to read the signal quality
    err = nrf_modem_at_cmd(responseBuffer, sizeof(responseBuffer), "AT+CESQ");
    if (err) 
    {
        LOG_ERR("Error sending AT command: %d\n", err);
        return -1;
    }

    // Validate the response format
    if (strstr(responseBuffer, "+CESQ:") == NULL) 
    {
        LOG_ERR("Unexpected response format: %s\n", responseBuffer);
        return -2; // Indicate a response format error
    }

    // Parse the response to extract values
    int parsedValues = sscanf(responseBuffer, "%*[^:]: %hhu,%hhu,%hhu,%hhu,%hhu,%hhu", rxlev, ber, rscp, ecno, rsrq, rsrp);
    if (parsedValues != 6) 
    {
        LOG_ERR("Error parsing response: %s\n", responseBuffer);
        return -3; // Indicate a parsing error
    }

    return 0; // Success
}