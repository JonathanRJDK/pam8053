#ifndef ENERGY_METER_H
#define ENERGY_METER_H

//Global macros used by the .c module which needs to easily be modified by the user


//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope
typedef struct
{
    float voltage;      // Voltage in Volts
    float current;      // Current in Amperes
    float power;        // Power in Watts
    float energy;       // Energy in Watt-hours
    float frequency;    // Frequency in Hertz
    float powerFactor;  // Power factor (dimensionless)
} measurementsStruct;

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int EnergyMeterInit();
int EnergyMeterStart();
int EnergyMeterGetMeasurements(measurementsStruct *measurements);
#ifdef __cplusplus
}
#endif

#endif //ENERGY_METER_H
