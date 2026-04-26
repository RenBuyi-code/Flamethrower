#ifndef HAL_IF_I_TYPES_H
#define HAL_IF_I_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  DMX_MODE_2CH = 2,
  DMX_MODE_6CH = 6
} dmx_mode_t;

typedef enum
{
  APP_MODE_TEST = 0,
  APP_MODE_USER = 1
} app_mode_t;

typedef struct
{
  bool oil_pump_on;
  bool oil_lock_valve_on;
  bool relief_valve_on;
  bool igniter_on;
  bool led_error_on;
  bool led_oil_pump_on;
  bool led_dmx_on;
  bool led_power_on;
  bool led_mode_on;
} actuator_output_t;

typedef struct
{
  uint16_t dmx_address;
  dmx_mode_t dmx_mode;
  uint16_t igniter_delay_ms;
  uint16_t oil_lock_delay_ms;
  bool tilt_protect_enable;
  uint8_t language;
} system_params_t;

typedef enum
{
  SENSOR_PRESSURE = 0,
  SENSOR_POWER1 = 1,
  SENSOR_POWER2 = 2,
  SENSOR_POWER3 = 3
} sensor_id_t;

typedef enum
{
  INPUT_SAFETY_LOCK = 0,
  INPUT_TILT_SWITCH = 1,
  INPUT_KEY_MENU = 2,
  INPUT_KEY_DOWN = 3,
  INPUT_KEY_UP = 4,
  INPUT_KEY_ENTER = 5
} input_id_t;

typedef struct
{
  bool request_fire;
  bool request_relief;
  uint16_t fire_duration_ms;
} dmx_intent_t;

#endif
