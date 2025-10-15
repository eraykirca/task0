#ifndef __EMERGENCY_MODULE__
#define __EMERGENCY_MODULE__

#include <stdint.h>

#define NUM_EMERGENCY_BUFFER 8

typedef struct {
  uint8_t emergency_buffer[NUM_EMERGENCY_BUFFER];
  uint32_t emergency_counter;
}EmergencyNode_t;

int8_t EmergencyNode_class_init(void);

int8_t
EmergencyNode_init(EmergencyNode_t* const restrict)__attribute__((__nonnull__(1)));

int8_t
EmergencyNode_raise(EmergencyNode_t* const restrict, const uint8_t exeception)__attribute__((__nonnull__(1)));

int8_t
EmergencyNode_solve(EmergencyNode_t* const restrict, const uint8_t exeception)__attribute__((__nonnull__(1)));

int8_t
EmergencyNode_is_emergency_state(const EmergencyNode_t* const restrict) __attribute__((__nonnull__(1)));

int8_t
EmergencyNode_destroy(EmergencyNode_t* const restrict)__attribute__((__nonnull__(1)));

#endif // !__EMERGENCY_MODULE__
