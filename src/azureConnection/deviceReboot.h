#ifndef DEVICE_REBOOT_H
#define DEVICE_REBOOT_H

//Global macros used by the .c module which needs to easily be modified by the user
#define GLOBAL_MACRO1 1
#define GLOBAL_MACRO2 2

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
 #include <zephyr/kernel.h>
 
//Global variables that needs to be accessed outside the modules scope

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
void DeviceReboot(const bool error);
 
 static inline void DeviceRebootError(void)
 {
   DeviceReboot(true);
 }
 
 static inline void DeviceRebootNormal(void)
 {
   DeviceReboot(false);
 }

#ifdef __cplusplus
}
#endif

#endif //DEVICE_REBOOT_H
