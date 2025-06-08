#include "stdint.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "zigbeeManager.h"

LOG_MODULE_REGISTER(zigbeeManager, CONFIG_LOG_DEFAULT_LEVEL);

float latestTempReading = 0.0f;

int ZigbeeManager(zigbeeManagerEventHandlerCb *zigbeeManagerEventHandler);

int ZigbeeManagerConnect(uint16_t timeoutSeconds);

int ZigbeeManagerGetTemp(uint16_t timeoutSeconds)
{
    return latestTempReading;
}


int ZigbeeManagerManagerDisconnect();