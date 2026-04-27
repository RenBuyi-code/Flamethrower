#include "app_selftest.h"
#include "log_rtt.h"
#include "../cfg/system_config.h"
#include "rules/dmx_strategy.h"
#include "rules/fault_manager.h"
#include "rules/safety_guard.h"
#include "rules/state_machine.h"
#include "../middleware/easyDMX/easy_dmx.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define APP_RULES_SELFTEST_ENABLE 1U

static bool app_selftest_state_machine(void)
{
  state_machine_t st;
  bool ok;

  state_machine_init(&st);
  ok = state_machine_transition(&st, MACHINE_SELFTEST, 0x0001U);
  ok = ok && state_machine_transition(&st, MACHINE_READY, 0x0002U);
  ok = ok && state_machine_transition(&st, MACHINE_FIRING, 0x0003U);
  ok = ok && state_machine_transition(&st, MACHINE_READY, 0x0004U);
  ok = ok && (state_machine_transition(&st, MACHINE_BOOT, 0x00FFU) == false);
  return ok;
}

static bool app_selftest_dmx_strategy(void)
{
  uint8_t ch[512];
  dmx_intent_t it;
  bool ok;

  memset(ch, 0, sizeof(ch));

  ch[0] = 253U;
  ch[1] = 50U;
  ok = dmx_strategy_build_intent(DMX_MODE_2CH, ch, 1U, &it);
  ok = ok && (it.request_fire == false) && (it.request_relief == false);

  ch[0] = 254U;
  ch[1] = 50U;
  ok = ok && dmx_strategy_build_intent(DMX_MODE_2CH, ch, 1U, &it);
  ok = ok && (it.request_fire == true) && (it.request_relief == false);

  ch[0] = 254U;
  ch[1] = 49U;
  ok = ok && dmx_strategy_build_intent(DMX_MODE_2CH, ch, 1U, &it);
  ok = ok && (it.request_relief == true);

  memset(ch, 0, sizeof(ch));
  ch[2] = 254U;
  ch[3] = 1U;
  ch[5] = 50U;
  ok = ok && dmx_strategy_build_intent(DMX_MODE_6CH, ch, 1U, &it);
  ok = ok && (it.request_fire == true) && (it.request_relief == false) && (it.fire_duration_ms == 10U);

  ch[3] = 255U;
  ok = ok && dmx_strategy_build_intent(DMX_MODE_6CH, ch, 1U, &it);
  ok = ok && (it.fire_duration_ms == 0U);

  ch[5] = 201U;
  ok = ok && dmx_strategy_build_intent(DMX_MODE_6CH, ch, 1U, &it);
  ok = ok && (it.request_relief == true);
  ok = ok && dmx_strategy_is_valid_start_address(DMX_MODE_2CH, 511U);
  ok = ok && (dmx_strategy_is_valid_start_address(DMX_MODE_2CH, 512U) == false);
  ok = ok && dmx_strategy_is_valid_start_address(DMX_MODE_6CH, 507U);
  ok = ok && (dmx_strategy_is_valid_start_address(DMX_MODE_6CH, 508U) == false);

  return ok;
}

static bool app_selftest_fault_manager(void)
{
  fault_manager_t fm;
  bool ok;

  fault_manager_init(&fm);
  fault_manager_set(&fm, FAULT_E1_PRESSURE_BUILD);
  ok = fault_manager_is_latched(&fm, FAULT_E1_PRESSURE_BUILD);
  fault_manager_try_clear(&fm, FAULT_E1_PRESSURE_BUILD, false);
  ok = ok && fault_manager_is_latched(&fm, FAULT_E1_PRESSURE_BUILD);
  fault_manager_try_clear(&fm, FAULT_E1_PRESSURE_BUILD, true);
  ok = ok && (fault_manager_is_latched(&fm, FAULT_E1_PRESSURE_BUILD) == false);
  return ok;
}

static bool app_selftest_safety_guard(void)
{
  safety_eval_input_t in;
  bool ok;

  memset(&in, 0, sizeof(in));
  in.dmx_online = 1;
  in.in_user_mode = 1;
  in.voltage_ok = 1;
  in.pressure_pct = CFG_PRESSURE_FIRE_MIN_PCT;
  in.pressure_fire_min_pct = CFG_PRESSURE_FIRE_MIN_PCT;
  ok = (safety_guard_eval(&in) == SAFETY_ALLOW_FIRE);

  in.relief_requested = 1;
  ok = ok && (safety_guard_eval(&in) == SAFETY_FORCE_RELIEF);
  in.relief_requested = 0;

  in.tilt_fault_active = 1;
  ok = ok && (safety_guard_eval(&in) == SAFETY_FORCE_STOP);
  in.tilt_fault_active = 0;

  in.in_user_mode = 0;
  ok = ok && (safety_guard_eval(&in) == SAFETY_LOCKED);
  return ok;
}

static bool app_selftest_easy_dmx(void)
{
  edmx_rx_t rx;
  edmx_event_t evt;
  edmx_frame_t frame;
  uint8_t fifo_storage[32];
  bool ok;

  ok = edmx_rx_init(&rx, fifo_storage, sizeof(fifo_storage), 500U);
  if(ok == false)
  {
    return false;
  }

  evt.byte = 0U;
  evt.flags = EDMX_EVENT_FLAG_BREAK;
  ok = edmx_rx_push_event(&rx, &evt);
  evt.byte = 0U;
  evt.flags = 0U;
  ok = ok && edmx_rx_push_event(&rx, &evt);
  evt.byte = 254U;
  ok = ok && edmx_rx_push_event(&rx, &evt);
  evt.byte = 50U;
  ok = ok && edmx_rx_push_event(&rx, &evt);
  evt.byte = 0U;
  evt.flags = EDMX_EVENT_FLAG_BREAK;
  ok = ok && edmx_rx_push_event(&rx, &evt);

  edmx_rx_process(&rx, 100U);
  ok = ok && edmx_rx_copy_latest(&rx, &frame);
  ok = ok && (frame.channels[0] == 254U);
  ok = ok && (frame.channels[1] == 50U);
  ok = ok && edmx_rx_is_online(&rx, 150U);
  ok = ok && (edmx_rx_is_online(&rx, 700U) == false);
  return ok;
}

void app_run_rules_selftests(void)
{
#if (APP_RULES_SELFTEST_ENABLE != 0U)
  typedef bool (*selftest_fn_t)(void);
  typedef struct
  {
    const char *name;
    selftest_fn_t fn;
  } selftest_item_t;

  const selftest_item_t items[] =
  {
    { "machine", app_selftest_state_machine },
    { "dmx", app_selftest_dmx_strategy },
    { "fault", app_selftest_fault_manager },
    { "safety", app_selftest_safety_guard },
    { "edmx", app_selftest_easy_dmx }
  };
  bool results[sizeof(items) / sizeof(items[0])];
  uint8_t i;

  for(i = 0U; i < (uint8_t)(sizeof(items) / sizeof(items[0])); i++)
  {
    results[i] = items[i].fn();
  }

  APP_LOGI("selftest machine=%u dmx=%u fault=%u safety=%u edmx=%u",
           (unsigned)results[0],
           (unsigned)results[1],
           (unsigned)results[2],
           (unsigned)results[3],
           (unsigned)results[4]);

  for(i = 0U; i < (uint8_t)(sizeof(items) / sizeof(items[0])); i++)
  {
    if(results[i] == false)
    {
      APP_LOGW("selftest fail: %s", items[i].name);
    }
  }
#endif
}
