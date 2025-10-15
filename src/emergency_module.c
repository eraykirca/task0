#include "./emergency_module.h"
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

//private

typedef atomic_ushort gpio;

gpio emergency_led;

static struct{
  atomic_flag lock;
  uint8_t excepion_counter;
  uint8_t init_done:1;
}EXCEPTION_COUNTER;

static inline void _hw_raise_emergency(void)
{
  while (atomic_flag_test_and_set(&EXCEPTION_COUNTER.lock));
  atomic_store(&emergency_led, 1);
  atomic_flag_clear(&EXCEPTION_COUNTER.lock);
}

static void _increase_global_emergency_counter(void) 
{
  while (atomic_flag_test_and_set(&EXCEPTION_COUNTER.lock));
  EXCEPTION_COUNTER.excepion_counter++;
  atomic_flag_clear(&EXCEPTION_COUNTER.lock);
}

static void _solved_module_exception_state(void)__attribute__((__unused__));
static void _solved_module_exception_state(void)
{
  while (atomic_flag_test_and_set(&EXCEPTION_COUNTER.lock));
  EXCEPTION_COUNTER.excepion_counter--;
  if (!EXCEPTION_COUNTER.excepion_counter)
  {
    atomic_store(&emergency_led, 0);
  }
  atomic_flag_clear(&EXCEPTION_COUNTER.lock);
}

static uint8_t read_globla_emergency_couner(void)__attribute__((__unused__));
static uint8_t read_globla_emergency_couner(void)
{
  while (atomic_flag_test_and_set(&EXCEPTION_COUNTER.lock));
  const uint8_t res= EXCEPTION_COUNTER.excepion_counter;
  atomic_flag_clear(&EXCEPTION_COUNTER.lock);

  return res;
}

//public

int8_t EmergencyNode_class_init(void)
{
  if (EXCEPTION_COUNTER.init_done)
  {
    return -1;
  }
  atomic_store(&emergency_led, 0);
  EXCEPTION_COUNTER.init_done=1;
  EXCEPTION_COUNTER.excepion_counter=0;

  return 0;
}

int8_t EmergencyNode_init(EmergencyNode_t* const restrict p_self)
{
  memset(p_self, 0, sizeof(*p_self));
  return 0;
}

int8_t EmergencyNode_raise(EmergencyNode_t* const restrict p_self, const uint8_t exeception)
{
  const uint8_t exception_byte = exeception/8;
  const uint8_t exception_bit = exeception % 8;

  if (exeception >= NUM_EMERGENCY_BUFFER*8)
  {
    return -1;
  }

  const uint8_t old_emergency_bit = (p_self->emergency_buffer[exception_byte] >> exception_bit) & 0x01;
  p_self->emergency_buffer[exception_byte] |= 1 << exception_bit;

  if (!old_emergency_bit)
  {
    if(!p_self->emergency_counter) {
      _increase_global_emergency_counter();
    }
    p_self->emergency_counter++;
  }

  _hw_raise_emergency();

  return 0;
}

int8_t EmergencyNode_solve(EmergencyNode_t* const restrict p_self, const uint8_t exeception)
{
  const uint8_t exception_byte = exeception/8;
  const uint8_t exception_bit = exeception % 8;

  if (exeception >= NUM_EMERGENCY_BUFFER * 8)
  {
    return -1;
  }

  if (p_self->emergency_buffer[exception_byte] &  (1 << exception_bit))
  {
    p_self->emergency_buffer[exception_byte] ^= (1 << exception_bit);
    p_self->emergency_counter--;
    if (!p_self->emergency_counter)
    {
      _solved_module_exception_state();
    }
  }

  return 0;
}

int8_t EmergencyNode_is_emergency_state(const EmergencyNode_t* const restrict p_self)
{
  return p_self->emergency_counter || read_globla_emergency_couner();
}

int8_t EmergencyNode_destroy(EmergencyNode_t* const restrict p_self)
{
  if (p_self->emergency_counter)
  {
    _solved_module_exception_state();
  }

  memset(p_self, 0, sizeof(*p_self));
  return 0;
}
