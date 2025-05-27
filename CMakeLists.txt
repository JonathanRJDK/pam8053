#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

#list(APPEND OVERLAY_CONFIG "$ENV{ZEPHYR_BASE}/../modules/lib/onomondo-softsim/overlay-softsim.conf")

macro(app_set_runner_args)
  board_runner_args(dfu-util "--dfuse-modifiers=force:mass-erase")
  board_runner_args(pyocd "--erase")
  board_runner_args(nrfjprog "--erase")
endmacro()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(PAT8002)

# NORDIC SDK APP START
# AzureConnection
target_sources(app PRIVATE src/azureConnection/azureManager.c)
target_sources(app PRIVATE src/azureConnection/deviceReboot.c)
target_sources(app PRIVATE src/azureConnection/deviceSettings.c)
target_sources(app PRIVATE src/azureConnection/l4ConnectionManager.c)
target_sources(app PRIVATE src/azureConnection/modemCommunicator.c)
target_sources(app PRIVATE src/azureConnection/nrfProvisioningAzure.c)
target_sources(app PRIVATE src/azureConnection/pam8053AzureDeviceTwin.c)

# externalControl
target_sources(app PRIVATE src/externalControl/relayControl.c)

# rs485
target_sources(app PRIVATE src/rs485/codePanel.c)
target_sources(app PRIVATE src/rs485/energyMeter.c)
target_sources(app PRIVATE src/rs485/rs485Communication.c)

# test
target_sources(app PRIVATE src/test/pam8053SelfTest.c)

# universalAlarmInput
target_sources(app PRIVATE src/universalAlarmInput/adcDts.c)
target_sources(app PRIVATE src/universalAlarmInput/universalAlarmInput.c)

# userInterface
target_sources(app PRIVATE src/userInterface/buttonModule.c)
target_sources(app PRIVATE src/userInterface/indicatorModule.c)

# zigbee
target_sources(app PRIVATE src/zigbee/zigbeeManager.c)


# main
target_sources(app PRIVATE src/main.c)
# NORDIC SDK APP END


zephyr_include_directories(src)
