#ifndef COMMON_H_
#define COMMON_H_

#include "ddl.h"


enum device_state_t {SLEEP, WAKEUP, RUNNING, PAUSE};

extern const uint32_t l_duration[18];

extern const uint32_t s_duration[18];

extern const float freq[18];

extern const uint32_t dacCur_r[18];

extern const int dacCur[9];

extern uint16_t dacCal[9];

#endif