#ifndef PAM8053_SELF_TEST_H
#define PAM8053_SELF_TEST_H

//Global macros used by the .c module which needs to easily be modified by the user
#define GLOBAL_MACRO1 1
#define GLOBAL_MACRO2 2

//Include libraries needed for the header to compile, often simple libraries like inttypes.h
#include <inttypes.h>

//Global variables that needs to be accessed outside the modules scope

#ifdef __cplusplus
extern "C" {
#endif
//Functions that should be accessible from the outside 
int Pmi8002SelfTestInit();

int Pmi8002SelfTestStart();

#ifdef __cplusplus
}
#endif

#endif //PAM8053_SELF_TEST_H
