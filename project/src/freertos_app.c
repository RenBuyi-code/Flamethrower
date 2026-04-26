/* add user code begin Header */
/**
  ******************************************************************************
  * File Name          : freertos_app.c
  * Description        : RTOS application with safety-first architecture
  */
/* add user code end Header */

#include "freertos_app.h"
#include "../inc/at32f415_conf.h"
#include "../../app/app_core.h"
#include "../../app/log_rtt.h"
#include "../../cfg/system_config.h"
#include "../../domain/dmx_strategy.h"
#include "../../domain/fault_manager.h"
#include "../../domain/safety_guard.h"
#include "../../middleware/easyDMX/easy_dmx.h"
#include "../../middleware/MultiButton/multi_button.h"

#define SL_PAGE_TRANSITION_MS 0

#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_event.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/core/inc/sl_page_manager.h"
#include "../../middleware/SlateUI/font/sl_font.h"
#include "../../middleware/SlateUI/port/sl_port.h"
#include "../../middleware/SlateUI/menu/inc/sl_menu_model.h"
#include "../../middleware/SlateUI/menu/inc/sl_menu_page.h"
#include "../../app/ui_pages/ui_idle_page.h"
#include "../../app/ui_pages/ui_main_menu.h"
#include "../../app/ui_pages/ui_safety_page.h"
#include "../../app/ui_pages/ui_setting_page.h"
#include "../../app/ui_pages/ui_language_page.h"
#include "../../app/ui_pages/ui_splash_page.h"
#include "../../app/ui_pages/ui_checking_page.h"
#include <string.h>

typedef enum
{
  ACT_CMD_SAFE_OFF = 0,
  ACT_CMD_RELIEF,
  ACT_CMD_FIRE,
  ACT_CMD_PUMP_ONLY
} actuator_cmd_type_t;

typedef struct
{
  actuator_cmd_type_t type;
  uint8_t priority;
  bool user_mode;
  uint16_t igniter_delay_ms;
  uint16_t oil_lock_delay_ms;
  uint16_t fire_duration_ms;
} actuator_cmd_t;

typedef struct
{
  actuator_output_t out;
  uint32_t tick_ms;
  bool fire_active;
  bool relief_active;
} actuator_status_t;

/* watchdog switch: keep disabled during debug bring-up */
#define APP_WDT_ENABLE                    0U
#define APP_WDT_DIVIDER                   WDT_CLK_DIV_256
#define APP_WDT_RELOAD                    1000U
#define APP_DOMAIN_SELFTEST_ENABLE        1U

enum
{
  EVT_STATE_READY_BIT = (1UL << 0),
  EVT_STATE_FIRING_BIT = (1UL << 1),
  EVT_STATE_RELIEF_BIT = (1UL << 2),
  EVT_STATE_FAULT_BIT = (1UL << 3),
  EVT_STATE_LOCKED_BIT = (1UL << 4),

  EVT_DMX_ONLINE_BIT = (1UL << 5),
  EVT_FAULT_E1_BIT = (1UL << 6),
  EVT_FAULT_E2_BIT = (1UL << 7),
  EVT_FAULT_E3_BIT = (1UL << 8),
  EVT_FAULT_E4_BIT = (1UL << 9),
  EVT_FAULT_E5_BIT = (1UL << 10),

  EVT_HB_SAFETY_BIT = (1UL << 11),
  EVT_HB_CONTROL_BIT = (1UL << 12),
  EVT_HB_ACTUATOR_BIT = (1UL << 13),
  EVT_HB_DMX_BIT = (1UL << 14),
  EVT_HB_UI_BIT = (1UL << 15),
  EVT_HB_DIAG_BIT = (1UL << 16),

  EVT_HB_MASK = EVT_HB_SAFETY_BIT | EVT_HB_CONTROL_BIT | EVT_HB_ACTUATOR_BIT | EVT_HB_DMX_BIT | EVT_HB_UI_BIT
};

/* task handler */
TaskHandle_t safety_handle;
TaskHandle_t control_handle;
TaskHandle_t actuator_handle;
TaskHandle_t dmx_handle;
TaskHandle_t ui_handle;
TaskHandle_t diag_handle;

/* Idle task control block and stack */
static StackType_t idle_task_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t idle_task_tcb;

static StaticTask_t safety_tcb;
static StaticTask_t control_tcb;
static StaticTask_t actuator_tcb;
static StaticTask_t dmx_tcb;
static StaticTask_t ui_tcb;
static StaticTask_t diag_tcb;

static StackType_t safety_stack[320];
static StackType_t control_stack[384];
static StackType_t actuator_stack[320];
static StackType_t dmx_stack[256];
static StackType_t ui_stack[256];
static StackType_t diag_stack[256];

static StaticQueue_t actuator_queue_tcb;
static uint8_t actuator_queue_storage[8 * sizeof(actuator_cmd_t)];

static StaticQueue_t actuator_status_queue_tcb;
static uint8_t actuator_status_storage[sizeof(actuator_status_t)];

static StaticEventGroup_t evt_group_tcb;

static QueueHandle_t q_actuator;
static QueueHandle_t q_actuator_status;
static EventGroupHandle_t eg_system;

static app_core_t g_app;
static edmx_rx_t s_dmx_rx;
static uint8_t s_dmx_fifo_storage[1024];

static int16_t s_shadow_dmx_addr;
static int16_t s_shadow_dmx_mode;
static int16_t s_shadow_ign_delay;
static int16_t s_shadow_lock_delay;
static int16_t s_shadow_tilt_enable;

static uint32_t g_ui_perf_last_cycles;
static uint32_t g_ui_perf_monotonic_us;
static uint32_t g_ui_perf_cycles_per_us;
static bool g_ui_perf_dwt_ready;
static bool g_ui_menu_active;

static void app_params_commit(void);
static void selftest_set_mode_state(bool user_mode, TickType_t now);
static void run_startup_selftest(void);
#if (configSUPPORT_STATIC_ALLOCATION == 1)
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
  if(ppxIdleTaskTCBBuffer != 0)
  {
    *ppxIdleTaskTCBBuffer = &idle_task_tcb;
  }
  if(ppxIdleTaskStackBuffer != 0)
  {
    *ppxIdleTaskStackBuffer = idle_task_stack;
  }
  if(pulIdleTaskStackSize != 0)
  {
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  }
}
#endif

static void ui_perf_init(void)
{
  uint32_t cpu_hz;

  cpu_hz = (uint32_t)system_core_clock;
  if(cpu_hz < 1000000U)
  {
    cpu_hz = 1000000U;
  }
  g_ui_perf_cycles_per_us = cpu_hz / 1000000U;
  if(g_ui_perf_cycles_per_us == 0U)
  {
    g_ui_perf_cycles_per_us = 1U;
  }

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  g_ui_perf_last_cycles = DWT->CYCCNT;
  g_ui_perf_monotonic_us = 0U;
  g_ui_perf_dwt_ready = ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U);
}

static uint32_t ui_perf_now_us(void)
{
  if(g_ui_perf_dwt_ready)
  {
    uint32_t cur_cycles;
    uint32_t delta_cycles;
    cur_cycles = DWT->CYCCNT;
    delta_cycles = cur_cycles - g_ui_perf_last_cycles;
    g_ui_perf_last_cycles = cur_cycles;
    g_ui_perf_monotonic_us += (delta_cycles / g_ui_perf_cycles_per_us);
    return g_ui_perf_monotonic_us;
  }
  return (uint32_t)xTaskGetTickCount() * 1000U;
}

static void app_wdt_init(void)
{
#if (APP_WDT_ENABLE != 0U)
  wdt_register_write_enable(TRUE);
  wdt_divider_set(APP_WDT_DIVIDER);
  wdt_reload_value_set(APP_WDT_RELOAD);
  while((wdt_flag_get(WDT_DIVF_UPDATE_FLAG) == SET) || (wdt_flag_get(WDT_RLDF_UPDATE_FLAG) == SET))
  {
  }
  wdt_counter_reload();
  wdt_enable();
#endif
}

static void app_wdt_feed(void)
{
#if (APP_WDT_ENABLE != 0U)
  wdt_counter_reload();
#endif
}

static void app_params_commit(void)
{
  cfg_sanitize_params(&g_app.params);
  (void)g_app.hal.storage.save_params(g_app.hal.storage.ctx, &g_app.params);
}

static bool app_selftest_machine_state(void)
{
  machine_state_ctx_t st;
  bool ok;

  machine_state_init(&st);
  ok = machine_state_transition(&st, MACHINE_SELFTEST, 0x0001U);
  ok = ok && machine_state_transition(&st, MACHINE_READY, 0x0002U);
  ok = ok && machine_state_transition(&st, MACHINE_FIRING, 0x0003U);
  ok = ok && machine_state_transition(&st, MACHINE_READY, 0x0004U);
  ok = ok && (machine_state_transition(&st, MACHINE_BOOT, 0x00FFU) == false);
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

static void app_run_domain_selftests(void)
{
#if (APP_DOMAIN_SELFTEST_ENABLE != 0U)
  bool s1;
  bool s2;
  bool s3;
  bool s4;
  bool s5;

  s1 = app_selftest_machine_state();
  s2 = app_selftest_dmx_strategy();
  s3 = app_selftest_fault_manager();
  s4 = app_selftest_safety_guard();
  s5 = app_selftest_easy_dmx();

  APP_LOGI("selftest machine=%u dmx=%u fault=%u safety=%u edmx=%u",
           (unsigned)s1,
           (unsigned)s2,
           (unsigned)s3,
           (unsigned)s4,
           (unsigned)s5);
#endif
}

typedef enum
{
  TEST_ACT_SAFE_OFF = 0,
  TEST_ACT_PUMP_ONLY,
  TEST_ACT_RELIEF,
  TEST_ACT_FIRE
} test_action_t;
static void queue_send_latest(QueueHandle_t q, const actuator_cmd_t *cmd, BaseType_t to_front)
{
  actuator_cmd_t dropped;
  if((q == 0) || (cmd == 0))
  {
    return;
  }

  if(to_front != pdFALSE)
  {
    if(xQueueSendToFront(q, cmd, 0) == pdPASS)
    {
      return;
    }
  }
  else
  {
    if(xQueueSend(q, cmd, 0) == pdPASS)
    {
      return;
    }
  }

  (void)xQueueReceive(q, &dropped, 0);
  if(to_front != pdFALSE)
  {
    (void)xQueueSendToFront(q, cmd, 0);
  }
  else
  {
    (void)xQueueSend(q, cmd, 0);
  }
}

static void set_state_bits(machine_state_t st)
{
  EventBits_t clear_mask;
  EventBits_t set_mask;
  clear_mask = EVT_STATE_READY_BIT | EVT_STATE_FIRING_BIT | EVT_STATE_RELIEF_BIT | EVT_STATE_FAULT_BIT | EVT_STATE_LOCKED_BIT;
  set_mask = 0U;

  switch(st)
  {
    case MACHINE_READY:
      set_mask = EVT_STATE_READY_BIT;
      break;
    case MACHINE_FIRING:
      set_mask = EVT_STATE_FIRING_BIT;
      break;
    case MACHINE_RELIEF:
      set_mask = EVT_STATE_RELIEF_BIT;
      break;
    case MACHINE_FAULT:
      set_mask = EVT_STATE_FAULT_BIT;
      break;
    case MACHINE_LOCKED:
      set_mask = EVT_STATE_LOCKED_BIT;
      break;
    default:
      break;
  }

  (void)xEventGroupClearBits(eg_system, clear_mask);
  if(set_mask != 0U)
  {
    (void)xEventGroupSetBits(eg_system, set_mask);
  }
}

static void set_fault_bits(uint32_t mask)
{
  EventBits_t clear_mask;
  EventBits_t set_mask;
  clear_mask = EVT_FAULT_E1_BIT | EVT_FAULT_E2_BIT | EVT_FAULT_E3_BIT | EVT_FAULT_E4_BIT | EVT_FAULT_E5_BIT;
  set_mask = 0U;
  if((mask & FAULT_MASK_E1) != 0U) { set_mask |= EVT_FAULT_E1_BIT; }
  if((mask & FAULT_MASK_E2) != 0U) { set_mask |= EVT_FAULT_E2_BIT; }
  if((mask & FAULT_MASK_E3) != 0U) { set_mask |= EVT_FAULT_E3_BIT; }
  if((mask & FAULT_MASK_E4) != 0U) { set_mask |= EVT_FAULT_E4_BIT; }
  if((mask & FAULT_MASK_E5) != 0U) { set_mask |= EVT_FAULT_E5_BIT; }
  (void)xEventGroupClearBits(eg_system, clear_mask);
  if(set_mask != 0U)
  {
    (void)xEventGroupSetBits(eg_system, set_mask);
  }
}

static uint32_t read_fault_mask_from_events(EventBits_t bits)
{
  uint32_t mask;
  mask = 0U;
  if((bits & EVT_FAULT_E1_BIT) != 0U) { mask |= FAULT_MASK_E1; }
  if((bits & EVT_FAULT_E2_BIT) != 0U) { mask |= FAULT_MASK_E2; }
  if((bits & EVT_FAULT_E3_BIT) != 0U) { mask |= FAULT_MASK_E3; }
  if((bits & EVT_FAULT_E4_BIT) != 0U) { mask |= FAULT_MASK_E4; }
  if((bits & EVT_FAULT_E5_BIT) != 0U) { mask |= FAULT_MASK_E5; }
  return mask;
}

static void send_safe_off_high_prio(void)
{
  actuator_cmd_t cmd;
  cmd.type = ACT_CMD_SAFE_OFF;
  cmd.priority = 10U;
  cmd.user_mode = false;
  cmd.igniter_delay_ms = 0U;
  cmd.oil_lock_delay_ms = 0U;
  cmd.fire_duration_ms = 0U;
  queue_send_latest(q_actuator, &cmd, pdTRUE);
}

void ui_setting_on_dmx_addr_save(int16_t value)
{
  g_app.params.dmx_address = (uint16_t)value;
  APP_LOGI("dmx addr=%u", (unsigned)g_app.params.dmx_address);
  app_params_commit();
}

void ui_setting_on_dmx_mode_save(int16_t value)
{
  g_app.params.dmx_mode = (value == 1) ? DMX_MODE_6CH : DMX_MODE_2CH;
  APP_LOGI("dmx mode=%u", (unsigned)g_app.params.dmx_mode);
  app_params_commit();
}

void ui_setting_on_ign_delay_save(int16_t value)
{
  g_app.params.igniter_delay_ms = (uint16_t)value;
  APP_LOGI("ign delay=%u", (unsigned)g_app.params.igniter_delay_ms);
  app_params_commit();
}

void ui_setting_on_lock_delay_save(int16_t value)
{
  g_app.params.oil_lock_delay_ms = (uint16_t)value;
  APP_LOGI("lock delay=%u", (unsigned)g_app.params.oil_lock_delay_ms);
  app_params_commit();
}

void ui_setting_on_tilt_enable_save(int16_t value)
{
  g_app.params.tilt_protect_enable = (value != 0) ? true : false;
  APP_LOGI("tilt protect=%u", (unsigned)g_app.params.tilt_protect_enable);
  app_params_commit();
}

enum
{
  UI_BTN_ID_MENU = 1,
  UI_BTN_ID_DOWN = 2,
  UI_BTN_ID_UP = 3,
  UI_BTN_ID_ENTER = 4
};

typedef struct
{
  Button key_menu;
  Button key_down;
  Button key_up;
  Button key_enter;
  bool initialized;
} ui_button_ctx_t;

static ui_button_ctx_t g_ui_btn;

static uint8_t ui_button_level_read(uint8_t button_id)
{
  switch(button_id)
  {
    case UI_BTN_ID_MENU:
      return g_app.hal.input.read(g_app.hal.input.ctx, INPUT_KEY_MENU) ? 1U : 0U;
    case UI_BTN_ID_DOWN:
      return g_app.hal.input.read(g_app.hal.input.ctx, INPUT_KEY_DOWN) ? 1U : 0U;
    case UI_BTN_ID_UP:
      return g_app.hal.input.read(g_app.hal.input.ctx, INPUT_KEY_UP) ? 1U : 0U;
    case UI_BTN_ID_ENTER:
      return g_app.hal.input.read(g_app.hal.input.ctx, INPUT_KEY_ENTER) ? 1U : 0U;
    default:
      return 0U;
  }
}

static void ui_post_key_event(sl_EventType type, uint8_t source)
{
  sl_Event evt;
  evt.type = type;
  evt.param = 0;
  evt.source = source;
  (void)sl_event_post(&evt);
}

static void ui_btn_click_cb(Button *btn, void *user_data)
{
  (void)user_data;
  if(btn == 0)
  {
    return;
  }

  APP_LOGI("key btn_id=%u", (unsigned)btn->button_id);

  switch(btn->button_id)
  {
    case UI_BTN_ID_MENU:
      ui_post_key_event(SL_EVT_KEY_BACK, SL_EVT_SOURCE_RAW);
      break;
    case UI_BTN_ID_DOWN:
      ui_post_key_event(SL_EVT_KEY_DOWN, SL_EVT_SOURCE_RAW);
      break;
    case UI_BTN_ID_UP:
      ui_post_key_event(SL_EVT_KEY_UP, SL_EVT_SOURCE_RAW);
      break;
    case UI_BTN_ID_ENTER:
      ui_post_key_event(SL_EVT_KEY_ENTER, SL_EVT_SOURCE_RAW);
      break;
    default:
      break;
  }
}

static void ui_btn_repeat_cb(Button *btn, void *user_data)
{
  (void)user_data;
  if(btn == 0)
  {
    return;
  }

  if(btn->button_id == UI_BTN_ID_DOWN)
  {
    ui_post_key_event(SL_EVT_KEY_DOWN, SL_EVT_SOURCE_REPEAT);
  }
  else if(btn->button_id == UI_BTN_ID_UP)
  {
    ui_post_key_event(SL_EVT_KEY_UP, SL_EVT_SOURCE_REPEAT);
  }
}

static void ui_setup_once(void)
{
  if(g_ui_btn.initialized)
  {
    return;
  }

  s_shadow_dmx_addr = (int16_t)g_app.params.dmx_address;
  s_shadow_dmx_mode = (int16_t)(g_app.params.dmx_mode == DMX_MODE_6CH ? 1 : 0);
  s_shadow_ign_delay = (int16_t)g_app.params.igniter_delay_ms;
  s_shadow_lock_delay = (int16_t)g_app.params.oil_lock_delay_ms;
  s_shadow_tilt_enable = g_app.params.tilt_protect_enable ? 1 : 0;

  ui_setting_page_set_dmx_refs(&s_shadow_dmx_addr, &s_shadow_dmx_mode);
  ui_setting_page_set_pressure_refs(&s_shadow_ign_delay, &s_shadow_lock_delay);
  ui_safety_page_set_tilt_ref(&s_shadow_tilt_enable);

  sl_port_init();
  sl_port_input_init();

  {
    uint8_t m = ui_button_level_read(UI_BTN_ID_MENU);
    uint8_t d = ui_button_level_read(UI_BTN_ID_DOWN);
    uint8_t u = ui_button_level_read(UI_BTN_ID_UP);
    uint8_t e = ui_button_level_read(UI_BTN_ID_ENTER);
    APP_LOGI("btn init state M=%u D=%u U=%u E=%u", (unsigned)m, (unsigned)d, (unsigned)u, (unsigned)e);
  }

  button_init(&g_ui_btn.key_menu, ui_button_level_read, 1U, UI_BTN_ID_MENU);
  button_init(&g_ui_btn.key_down, ui_button_level_read, 1U, UI_BTN_ID_DOWN);
  button_init(&g_ui_btn.key_up, ui_button_level_read, 1U, UI_BTN_ID_UP);
  button_init(&g_ui_btn.key_enter, ui_button_level_read, 1U, UI_BTN_ID_ENTER);

  button_attach(&g_ui_btn.key_menu, BTN_PRESS_DOWN, ui_btn_click_cb, 0);
  button_attach(&g_ui_btn.key_down, BTN_PRESS_DOWN, ui_btn_click_cb, 0);
  button_attach(&g_ui_btn.key_down, BTN_LONG_PRESS_HOLD, ui_btn_repeat_cb, 0);
  button_attach(&g_ui_btn.key_up, BTN_PRESS_DOWN, ui_btn_click_cb, 0);
  button_attach(&g_ui_btn.key_up, BTN_LONG_PRESS_HOLD, ui_btn_repeat_cb, 0);
  button_attach(&g_ui_btn.key_enter, BTN_PRESS_DOWN, ui_btn_click_cb, 0);

  (void)button_start(&g_ui_btn.key_menu);
  (void)button_start(&g_ui_btn.key_down);
  (void)button_start(&g_ui_btn.key_up);
  (void)button_start(&g_ui_btn.key_enter);

  sl_lang_set((int)g_app.params.language);
  sl_disp_init();
  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);
  sl_disp_flush();
  sl_page_manager_init(ui_splash_page_get());

  g_ui_btn.initialized = true;
  APP_LOGI("ui setup done");
}

static void selftest_set_mode_state(bool user_mode, TickType_t now)
{
  if(user_mode)
  {
    fault_manager_try_clear(&g_app.faults, FAULT_E4_LOCKED_MODE, true);
    (void)app_core_switch_state(&g_app, MACHINE_READY, 0x2002U, (uint32_t)now);
  }
  else
  {
    fault_manager_set(&g_app.faults, FAULT_E4_LOCKED_MODE);
    (void)app_core_switch_state(&g_app, MACHINE_LOCKED, 0x2005U, (uint32_t)now);
  }
  set_fault_bits(g_app.faults.latched_mask);
  set_state_bits(g_app.machine.current);
}

static void run_startup_selftest(void)
{
  TickType_t start_tick;
  TickType_t now;
  bool user_mode;
  bool tilt_fault;
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  actuator_cmd_t cmd;

  start_tick = xTaskGetTickCount();
  cmd.type = ACT_CMD_SAFE_OFF;
  cmd.priority = 5U;
  cmd.user_mode = false;
  cmd.igniter_delay_ms = 0U;
  cmd.oil_lock_delay_ms = 0U;
  cmd.fire_duration_ms = 0U;
  queue_send_latest(q_actuator, &cmd, pdTRUE);

  for(;;)
  {
    now = xTaskGetTickCount();
    user_mode = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_SAFETY_LOCK);
    tilt_fault = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_TILT_SWITCH);
    pressure_raw = g_app.hal.adc.read_raw(g_app.hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);

    if(g_app.params.tilt_protect_enable && tilt_fault)
    {
      fault_manager_set(&g_app.faults, FAULT_E2_TILT);
      send_safe_off_high_prio();
      (void)app_core_switch_state(&g_app, MACHINE_FAULT, 0x2006U, (uint32_t)now);
      set_fault_bits(g_app.faults.latched_mask);
      set_state_bits(g_app.machine.current);
      return;
    }

    if(pressure_pct >= CFG_PRESSURE_TARGET_PCT)
    {
      cmd.type = ACT_CMD_SAFE_OFF;
      cmd.user_mode = user_mode;
      queue_send_latest(q_actuator, &cmd, pdTRUE);
      selftest_set_mode_state(user_mode, now);
      return;
    }

    if((now - start_tick) >= pdMS_TO_TICKS(CFG_SELFTEST_PRESSURE_TIMEOUT_MS))
    {
      fault_manager_set(&g_app.faults, FAULT_E1_PRESSURE_BUILD);
      send_safe_off_high_prio();
      (void)app_core_switch_state(&g_app, MACHINE_FAULT, 0x2007U, (uint32_t)now);
      set_fault_bits(g_app.faults.latched_mask);
      set_state_bits(g_app.machine.current);
      return;
    }

    cmd.type = ACT_CMD_PUMP_ONLY;
    cmd.user_mode = user_mode;
    queue_send_latest(q_actuator, &cmd, pdFALSE);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void safety_task(void *pvParameters)
{
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  uint16_t voltage_raw;
  bool user_mode;
  bool tilt_fault;
  TickType_t now;
  TickType_t e1_start;
  TickType_t e3_start;
  TickType_t e5_start;
  uint32_t last_fault_mask;
  actuator_status_t st;
  BaseType_t have_status;
  EventBits_t bits;
  (void)pvParameters;

  e1_start = 0U;
  e3_start = 0U;
  e5_start = 0U;
  last_fault_mask = 0U;

  for(;;)
  {
    now = xTaskGetTickCount();
    pressure_raw = g_app.hal.adc.read_raw(g_app.hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    voltage_raw = g_app.hal.adc.read_raw(g_app.hal.adc.ctx, SENSOR_POWER1);
    user_mode = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_SAFETY_LOCK);
    tilt_fault = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_TILT_SWITCH);

    have_status = xQueuePeek(q_actuator_status, &st, 0);
    bits = xEventGroupGetBits(eg_system);

    if((have_status == pdTRUE) &&
       st.out.oil_pump_on &&
       (st.fire_active == false) &&
       (st.relief_active == false) &&
       (pressure_pct < CFG_PRESSURE_TARGET_PCT))
    {
      if(e1_start == 0U)
      {
        e1_start = now;
      }
      else if((now - e1_start) >= pdMS_TO_TICKS(CFG_PRESSURE_ERROR_TIMEOUT_MS))
      {
        fault_manager_set(&g_app.faults, FAULT_E1_PRESSURE_BUILD);
      }
    }
    else
    {
      e1_start = 0U;
      fault_manager_try_clear(&g_app.faults, FAULT_E1_PRESSURE_BUILD, pressure_pct >= CFG_PRESSURE_TARGET_PCT);
    }

    if(g_app.params.tilt_protect_enable && tilt_fault)
    {
      fault_manager_set(&g_app.faults, FAULT_E2_TILT);
    }
    else
    {
      fault_manager_try_clear(&g_app.faults, FAULT_E2_TILT, true);
    }

    if(cfg_voltage_raw_in_range(voltage_raw) == false)
    {
      if(e3_start == 0U)
      {
        e3_start = now;
      }
      else if((now - e3_start) >= pdMS_TO_TICKS(CFG_VOLTAGE_ERROR_HOLD_MS))
      {
        fault_manager_set(&g_app.faults, FAULT_E3_VOLTAGE);
      }
    }
    else
    {
      e3_start = 0U;
      fault_manager_try_clear(&g_app.faults, FAULT_E3_VOLTAGE, true);
    }

    if((user_mode == false) && (g_app.machine.current != MACHINE_SELFTEST))
    {
      fault_manager_set(&g_app.faults, FAULT_E4_LOCKED_MODE);
    }
    else
    {
      fault_manager_try_clear(&g_app.faults, FAULT_E4_LOCKED_MODE, true);
    }

    if((have_status == pdTRUE) && st.out.relief_valve_on && (pressure_pct > CFG_PRESSURE_RELIEF_DONE_PCT))
    {
      if(e5_start == 0U)
      {
        e5_start = now;
      }
      else if((now - e5_start) >= pdMS_TO_TICKS(CFG_RELIEF_ERROR_TIMEOUT_MS))
      {
        fault_manager_set(&g_app.faults, FAULT_E5_RELIEF);
      }
    }
    else
    {
      e5_start = 0U;
      fault_manager_try_clear(&g_app.faults, FAULT_E5_RELIEF, pressure_pct <= CFG_PRESSURE_RELIEF_DONE_PCT);
    }

    set_fault_bits(g_app.faults.latched_mask);
    if(last_fault_mask != g_app.faults.latched_mask)
    {
      APP_LOGW("fault mask: 0x%02lX -> 0x%02lX",
               (unsigned long)last_fault_mask,
               (unsigned long)g_app.faults.latched_mask);
      last_fault_mask = g_app.faults.latched_mask;
    }

    if((g_app.faults.latched_mask & FAULT_MASK_FATAL) != 0U)
    {
      send_safe_off_high_prio();
      (void)app_core_switch_state(&g_app, MACHINE_FAULT, 0x2101U, (uint32_t)now);
      set_state_bits(g_app.machine.current);
    }
    else if((g_app.faults.latched_mask & FAULT_MASK_E4) != 0U)
    {
      (void)app_core_switch_state(&g_app, MACHINE_LOCKED, 0x2102U, (uint32_t)now);
      set_state_bits(g_app.machine.current);
    }

    if((user_mode == true) && ((bits & EVT_DMX_ONLINE_BIT) == 0U))
    {
      send_safe_off_high_prio();
    }

    (void)xEventGroupSetBits(eg_system, EVT_HB_SAFETY_BIT);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void control_task(void *pvParameters)
{
  TickType_t now;
  EventBits_t bits;
  actuator_cmd_t cmd;
  dmx_intent_t intent;
  safety_eval_input_t in;
  bool ok;
  bool user_mode;
  bool key_menu;
  bool key_down;
  bool key_up;
  bool key_enter;
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  test_action_t test_action;
  test_action_t last_test_action;
  edmx_frame_t frame;
  bool pressure_ready_for_fire;
  bool pressure_refill_active;
  (void)pvParameters;

  last_test_action = (test_action_t)0xFF;
  pressure_refill_active = true;

  /* Startup state SELFTEST is set in wk_freertos_init before tasks run. */
  vTaskDelay(pdMS_TO_TICKS(50));
  run_startup_selftest();

  for(;;)
  {
    now = xTaskGetTickCount();
    edmx_rx_process(&s_dmx_rx, (uint32_t)now);

    if(edmx_rx_is_online(&s_dmx_rx, (uint32_t)now))
    {
      (void)xEventGroupSetBits(eg_system, EVT_DMX_ONLINE_BIT);
    }
    else
    {
      (void)xEventGroupClearBits(eg_system, EVT_DMX_ONLINE_BIT);
    }

    bits = xEventGroupGetBits(eg_system);
    user_mode = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_SAFETY_LOCK);

    ok = edmx_rx_copy_latest(&s_dmx_rx, &frame);
    if(ok)
    {
      ok = dmx_strategy_build_intent(g_app.params.dmx_mode, frame.channels, g_app.params.dmx_address, &intent);
    }
    if(ok == false)
    {
      intent.request_fire = false;
      intent.request_relief = false;
      intent.fire_duration_ms = 0U;
      if((bits & EVT_DMX_ONLINE_BIT) != 0U)
      {
        APP_LOGW("dmx invalid -> safe_off");
      }
    }

    in.latched_fault_mask = read_fault_mask_from_events(bits);
    in.dmx_online = ((bits & EVT_DMX_ONLINE_BIT) != 0U) ? 1 : 0;
    in.relief_requested = intent.request_relief ? 1 : 0;
    in.fire_requested = intent.request_fire ? 1 : 0;
    in.in_user_mode = user_mode ? 1 : 0;
    in.tilt_fault_active = (g_app.params.tilt_protect_enable && g_app.hal.input.read(g_app.hal.input.ctx, INPUT_TILT_SWITCH)) ? 1 : 0;
    in.voltage_ok = 1;
    pressure_raw = g_app.hal.adc.read_raw(g_app.hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    in.pressure_pct = pressure_pct;
    in.pressure_fire_min_pct = CFG_PRESSURE_FIRE_MIN_PCT;

    cmd.priority = 1U;
    cmd.igniter_delay_ms = g_app.params.igniter_delay_ms;
    cmd.oil_lock_delay_ms = g_app.params.oil_lock_delay_ms;
    cmd.fire_duration_ms = intent.fire_duration_ms;
    cmd.user_mode = user_mode;

    if(user_mode == false)
    {
      if(g_ui_menu_active == true)
      {
        cmd.type = ACT_CMD_SAFE_OFF;
        queue_send_latest(q_actuator, &cmd, pdTRUE);
        (void)xEventGroupSetBits(eg_system, EVT_HB_CONTROL_BIT);
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }

      key_menu = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_KEY_MENU);
      key_down = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_KEY_DOWN);
      key_up = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_KEY_UP);
      key_enter = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_KEY_ENTER);

      if((in.latched_fault_mask & FAULT_MASK_FATAL) != 0U)
      {
        (void)app_core_switch_state(&g_app, MACHINE_FAULT, 0x2301U, (uint32_t)now);
        set_state_bits(g_app.machine.current);
        cmd.type = ACT_CMD_SAFE_OFF;
        queue_send_latest(q_actuator, &cmd, pdTRUE);
      }
      else
      {
        if(key_menu)
        {
          test_action = TEST_ACT_SAFE_OFF;
        }
        else if(key_down)
        {
          test_action = TEST_ACT_RELIEF;
        }
        else if(key_enter)
        {
          test_action = TEST_ACT_FIRE;
        }
        else if(key_up)
        {
          test_action = TEST_ACT_PUMP_ONLY;
        }
        else if(((bits & EVT_DMX_ONLINE_BIT) != 0U) && (ok == true))
        {
          if(intent.request_relief)
          {
            test_action = TEST_ACT_RELIEF;
          }
          else if(intent.request_fire)
          {
            test_action = TEST_ACT_FIRE;
          }
          else
          {
            test_action = TEST_ACT_SAFE_OFF;
          }
        }
        else
        {
          test_action = TEST_ACT_SAFE_OFF;
        }

        if(test_action != last_test_action)
        {
          switch(test_action)
          {
            case TEST_ACT_PUMP_ONLY:
              cmd.type = ACT_CMD_PUMP_ONLY;
              APP_LOGI("test mode action=PUMP");
              break;
            case TEST_ACT_RELIEF:
              cmd.type = ACT_CMD_RELIEF;
              APP_LOGI("test mode action=RELIEF");
              break;
            case TEST_ACT_FIRE:
              cmd.type = ACT_CMD_FIRE;
              APP_LOGI("test mode action=IGNITER_TEST");
              break;
            case TEST_ACT_SAFE_OFF:
            default:
              cmd.type = ACT_CMD_SAFE_OFF;
              APP_LOGI("test mode action=SAFE_OFF");
              break;
          }

          queue_send_latest(q_actuator, &cmd, (cmd.type == ACT_CMD_SAFE_OFF) ? pdTRUE : pdFALSE);
          last_test_action = test_action;
        }
      }

      (void)xEventGroupSetBits(eg_system, EVT_HB_CONTROL_BIT);
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    last_test_action = (test_action_t)0xFF;

    switch(safety_guard_eval(&in))
    {
      case SAFETY_FORCE_STOP:
        (void)app_core_switch_state(&g_app, MACHINE_FAULT, 0x2201U, (uint32_t)now);
        set_state_bits(g_app.machine.current);
        cmd.type = ACT_CMD_SAFE_OFF;
        queue_send_latest(q_actuator, &cmd, pdTRUE);
        break;
      case SAFETY_LOCKED:
        (void)app_core_switch_state(&g_app, MACHINE_LOCKED, 0x2202U, (uint32_t)now);
        set_state_bits(g_app.machine.current);
        cmd.type = ACT_CMD_SAFE_OFF;
        queue_send_latest(q_actuator, &cmd, pdTRUE);
        break;
      case SAFETY_FORCE_RELIEF:
        (void)app_core_switch_state(&g_app, MACHINE_RELIEF, 0x2203U, (uint32_t)now);
        set_state_bits(g_app.machine.current);
        cmd.type = ACT_CMD_RELIEF;
        queue_send_latest(q_actuator, &cmd, pdFALSE);
        break;
      case SAFETY_ALLOW_FIRE:
      default:
        pressure_ready_for_fire = (pressure_pct >= CFG_PRESSURE_FIRE_MIN_PCT);
        if(pressure_pct >= CFG_PRESSURE_TARGET_PCT)
        {
          pressure_refill_active = false;
        }
        else if(pressure_pct <= CFG_PRESSURE_REFILL_RESUME_PCT)
        {
          pressure_refill_active = true;
        }

        if(intent.request_relief)
        {
          (void)app_core_switch_state(&g_app, MACHINE_RELIEF, 0x2204U, (uint32_t)now);
          set_state_bits(g_app.machine.current);
          cmd.type = ACT_CMD_RELIEF;
          queue_send_latest(q_actuator, &cmd, pdFALSE);
        }
        else if(intent.request_fire && pressure_ready_for_fire)
        {
          cmd.type = ACT_CMD_FIRE;
          queue_send_latest(q_actuator, &cmd, pdFALSE);
          (void)app_core_switch_state(&g_app, MACHINE_FIRING, 0x2003U, (uint32_t)now);
          set_state_bits(g_app.machine.current);
        }
        else if(intent.request_fire)
        {
          cmd.type = ACT_CMD_PUMP_ONLY;
          queue_send_latest(q_actuator, &cmd, pdFALSE);
          (void)app_core_switch_state(&g_app, MACHINE_READY, 0x2008U, (uint32_t)now);
          set_state_bits(g_app.machine.current);
        }
        else
        {
          cmd.type = pressure_refill_active ? ACT_CMD_PUMP_ONLY : ACT_CMD_SAFE_OFF;
          queue_send_latest(q_actuator, &cmd, pdFALSE);
          (void)app_core_switch_state(&g_app, MACHINE_READY, 0x2004U, (uint32_t)now);
          set_state_bits(g_app.machine.current);
        }
        break;
    }

    (void)xEventGroupSetBits(eg_system, EVT_HB_CONTROL_BIT);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void actuator_task(void *pvParameters)
{
  actuator_output_t out;
  actuator_cmd_t cmd;
  BaseType_t got;
  TickType_t fire_start_tick;
  bool fire_active;
  bool relief_active;
  actuator_status_t status;
  TickType_t now;
  bool enable_igniter;
  bool enable_lock_valve;
  bool user_mode_latched;
  TickType_t elapsed;
  (void)pvParameters;

  out.oil_pump_on = false;
  out.oil_lock_valve_on = false;
  out.relief_valve_on = false;
  out.igniter_on = false;
  out.led_error_on = false;
  out.led_oil_pump_on = false;
  out.led_dmx_on = false;
  out.led_power_on = true;
  out.led_mode_on = false;

  fire_start_tick = 0U;
  fire_active = false;
  relief_active = false;
  user_mode_latched = g_app.hal.input.read(g_app.hal.input.ctx, INPUT_SAFETY_LOCK);

  for(;;)
  {
    got = xQueueReceive(q_actuator, &cmd, pdMS_TO_TICKS(10));
    now = xTaskGetTickCount();

    if(got == pdTRUE)
    {
      user_mode_latched = cmd.user_mode;
      switch(cmd.type)
      {
        case ACT_CMD_SAFE_OFF:
          fire_active = false;
          relief_active = false;
          out.oil_pump_on = false;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = false;
          out.igniter_on = false;
          break;

        case ACT_CMD_RELIEF:
          fire_active = false;
          relief_active = true;
          out.oil_pump_on = false;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = true;
          out.igniter_on = false;
          break;

        case ACT_CMD_PUMP_ONLY:
          fire_active = false;
          relief_active = false;
          out.oil_pump_on = true;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = false;
          out.igniter_on = false;
          break;

        case ACT_CMD_FIRE:
        default:
          fire_active = true;
          relief_active = false;
          fire_start_tick = now;
          out.oil_pump_on = true;
          out.relief_valve_on = false;
          out.oil_lock_valve_on = false;
          out.igniter_on = false;
          break;
      }
    }

    if(fire_active)
    {
      elapsed = now - fire_start_tick;
      enable_igniter = (elapsed >= pdMS_TO_TICKS(cmd.igniter_delay_ms));
      enable_lock_valve = cmd.user_mode && (elapsed >= pdMS_TO_TICKS(cmd.oil_lock_delay_ms));

      out.oil_pump_on = true;
      out.relief_valve_on = false;
      out.igniter_on = enable_igniter;
      out.oil_lock_valve_on = enable_lock_valve;

      if((cmd.fire_duration_ms > 0U) && (elapsed >= pdMS_TO_TICKS(cmd.fire_duration_ms)))
      {
        fire_active = false;
        out.oil_lock_valve_on = false;
        out.igniter_on = false;
      }
    }

    if(relief_active)
    {
      out.relief_valve_on = true;
      out.oil_pump_on = false;
      out.oil_lock_valve_on = false;
      out.igniter_on = false;
    }

    out.led_oil_pump_on = out.oil_pump_on;
    out.led_error_on = ((xEventGroupGetBits(eg_system) & (EVT_STATE_FAULT_BIT | EVT_FAULT_E1_BIT | EVT_FAULT_E2_BIT | EVT_FAULT_E3_BIT | EVT_FAULT_E5_BIT)) != 0U);
    out.led_dmx_on = ((xEventGroupGetBits(eg_system) & EVT_DMX_ONLINE_BIT) != 0U);
    out.led_power_on = true;
    out.led_mode_on = user_mode_latched;

    g_app.hal.actuator.apply(g_app.hal.actuator.ctx, &out);

    status.out = out;
    status.tick_ms = (uint32_t)now;
    status.fire_active = fire_active;
    status.relief_active = relief_active;
    (void)xQueueOverwrite(q_actuator_status, &status);

    (void)xEventGroupSetBits(eg_system, EVT_HB_ACTUATOR_BIT);
  }
}

void dmx_task(void *pvParameters)
{
  edmx_event_t evt;
  bool is_break;
  uint8_t b;
  (void)pvParameters;

  for(;;)
  {
    while(g_app.hal.dmx.poll_byte(g_app.hal.dmx.ctx, &b, &is_break))
    {
      evt.byte = b;
      evt.flags = is_break ? EDMX_EVENT_FLAG_BREAK : 0U;
      (void)edmx_rx_push_event(&s_dmx_rx, &evt);
    }
    (void)xEventGroupSetBits(eg_system, EVT_HB_DMX_BIT);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void ui_task(void *pvParameters)
{
  typedef enum
  {
    UI_FLOW_SPLASH = 0,
    UI_FLOW_CHECKING,
    UI_FLOW_IDLE
  } ui_flow_stage_t;

  static ui_flow_stage_t s_ui_flow_stage = UI_FLOW_SPLASH;
  TickType_t splash_start_tick;
  TickType_t checking_start_tick;
  (void)pvParameters;
  ui_setup_once();
  s_ui_flow_stage = UI_FLOW_SPLASH;
  splash_start_tick = xTaskGetTickCount();
  checking_start_tick = splash_start_tick;
  for(;;)
  {
    button_ticks();

    {
      EventBits_t bits;
      uint16_t pressure_raw;
      uint8_t pressure_pct;
      bool dmx_online;
      machine_state_t st;

      bits = xEventGroupGetBits(eg_system);
      pressure_raw = g_app.hal.adc.read_raw(g_app.hal.adc.ctx, SENSOR_PRESSURE);
      pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
      dmx_online = ((bits & EVT_DMX_ONLINE_BIT) != 0U);
      st = g_app.machine.current;

      {
        actuator_status_t act_st;
        BaseType_t act_ok;
        bool pumping;

        act_ok = xQueuePeek(q_actuator_status, &act_st, 0);
        pumping = ((act_ok == pdTRUE) && act_st.out.oil_pump_on && (act_st.fire_active == false) && (act_st.relief_active == false));
        ui_idle_page_update(st, dmx_online, pumping, pressure_pct, g_app.faults.latched_mask, g_app.params.dmx_address);
      }
      ui_checking_page_update(st, pressure_pct, g_app.faults.latched_mask);
    }

      sl_page_manager_process();
      sl_page_manager_tick((uint16_t)TICKS_INTERVAL);

      ui_splash_page_tick();
      if(s_ui_flow_stage == UI_FLOW_SPLASH)
      {
        TickType_t now_tick = xTaskGetTickCount();
        TickType_t splash_elapsed_tick = now_tick - splash_start_tick;

        if((splash_elapsed_tick >= pdMS_TO_TICKS(2000U)) && ui_splash_page_is_done())
        {
          uint32_t splash_elapsed_ms = (uint32_t)(splash_elapsed_tick * portTICK_PERIOD_MS);
          s_ui_flow_stage = UI_FLOW_CHECKING;
          checking_start_tick = now_tick;
          APP_LOGI("ui page: splash -> checking (%lums)", splash_elapsed_ms);
          sl_page_enter(ui_checking_page_get());
        }
      }
      else if(s_ui_flow_stage == UI_FLOW_CHECKING)
      {
        TickType_t now_tick = xTaskGetTickCount();
        TickType_t checking_elapsed_tick = now_tick - checking_start_tick;

        if((checking_elapsed_tick >= pdMS_TO_TICKS(5000U)) && ui_checking_page_is_done())
        {
          uint32_t checking_elapsed_ms = (uint32_t)(checking_elapsed_tick * portTICK_PERIOD_MS);
          s_ui_flow_stage = UI_FLOW_IDLE;
          APP_LOGI("ui page: checking -> idle (%lums)", checking_elapsed_ms);
          sl_page_enter(ui_idle_page_get());
        }
      }

    if(ui_main_menu_consume_back_to_idle())
    {
      APP_LOGI("ui page: back to idle");
      g_ui_menu_active = false;
    }

    if(ui_idle_page_consume_enter_menu())
    {
      APP_LOGI("ui page: idle -> main_menu");
      g_ui_menu_active = true;
      sl_page_enter(ui_main_menu_get());
    }

    {
      int lang = ui_language_page_consume_selection();
      if(lang >= 0)
      {
        APP_LOGI("ui lang: switched to %d", lang);
        g_app.params.language = (uint8_t)lang;
        app_params_commit();
      }
    }

    {
      int tilt = ui_safety_page_consume_tilt_changed();
      if(tilt >= 0)
      {
        ui_setting_on_tilt_enable_save((int16_t)tilt);
      }
    }

    if(g_ui_menu_active)
    {
      int sel = ui_main_menu_consume_selected();
      if(sel == UI_MENU_ITEM_DMX)
      {
        APP_LOGI("ui page: main_menu -> dmx_set");
        sl_page_enter(ui_setting_page_get_dmx());
      }
      else if(sel == UI_MENU_ITEM_PRESSURE)
      {
        APP_LOGI("ui page: main_menu -> pressure_set");
        sl_page_enter(ui_setting_page_get_pressure());
      }
      else if(sel == UI_MENU_ITEM_TILT)
      {
        APP_LOGI("ui page: main_menu -> tilt");
        sl_page_enter(ui_safety_page_get());
      }
      else if(sel == UI_MENU_ITEM_LANGUAGE)
      {
        APP_LOGI("ui page: main_menu -> language");
        sl_page_enter(ui_language_page_get());
      }
    }

    (void)xEventGroupSetBits(eg_system, EVT_HB_UI_BIT);
    vTaskDelay(pdMS_TO_TICKS(TICKS_INTERVAL));
  }
}

void diag_task(void *pvParameters)
{
  EventBits_t bits;
  TickType_t miss_since;
  actuator_cmd_t cmd;
  bool hb_ok;
  (void)pvParameters;
  miss_since = 0U;

  for(;;)
  {
    bits = xEventGroupGetBits(eg_system);
    hb_ok = ((bits & EVT_HB_MASK) == EVT_HB_MASK);
    if(hb_ok == false)
    {
      if(miss_since == 0U)
      {
        miss_since = xTaskGetTickCount();
      }
      else if((xTaskGetTickCount() - miss_since) >= pdMS_TO_TICKS(1000))
      {
        cmd.type = ACT_CMD_SAFE_OFF;
        cmd.priority = 10U;
        cmd.user_mode = false;
        cmd.igniter_delay_ms = 0U;
        cmd.oil_lock_delay_ms = 0U;
        cmd.fire_duration_ms = 0U;
        queue_send_latest(q_actuator, &cmd, pdTRUE);
      }
    }
    else
    {
      miss_since = 0U;
      app_wdt_feed();
    }

    (void)xEventGroupClearBits(eg_system, EVT_HB_MASK);
    (void)xEventGroupSetBits(eg_system, EVT_HB_DIAG_BIT);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void freertos_task_create(void)
{
  safety_handle = xTaskCreateStatic(safety_task, "safety", 320, 0, configMAX_PRIORITIES - 1U, safety_stack, &safety_tcb);
  control_handle = xTaskCreateStatic(control_task, "control", 384, 0, configMAX_PRIORITIES - 3U, control_stack, &control_tcb);
  actuator_handle = xTaskCreateStatic(actuator_task, "actuator", 320, 0, configMAX_PRIORITIES - 2U, actuator_stack, &actuator_tcb);
  dmx_handle = xTaskCreateStatic(dmx_task, "dmx", 256, 0, configMAX_PRIORITIES - 4U, dmx_stack, &dmx_tcb);
  ui_handle = xTaskCreateStatic(ui_task, "ui", 256, 0, configMAX_PRIORITIES - 6U, ui_stack, &ui_tcb);
  diag_handle = xTaskCreateStatic(diag_task, "diag", 256, 0, configMAX_PRIORITIES - 5U, diag_stack, &diag_tcb);
}

void wk_freertos_init(void)
{
  ui_perf_init();
  app_core_init(&g_app);
  app_core_load_or_default_params(&g_app);
  app_run_domain_selftests();

  q_actuator = xQueueCreateStatic(8, sizeof(actuator_cmd_t), actuator_queue_storage, &actuator_queue_tcb);
  q_actuator_status = xQueueCreateStatic(1, sizeof(actuator_status_t), actuator_status_storage, &actuator_status_queue_tcb);
  eg_system = xEventGroupCreateStatic(&evt_group_tcb);
  (void)edmx_rx_init(&s_dmx_rx, s_dmx_fifo_storage, sizeof(s_dmx_fifo_storage), CFG_DMX_LOST_TIMEOUT_MS);

  (void)app_core_switch_state(&g_app, MACHINE_SELFTEST, 0x2001U, (uint32_t)xTaskGetTickCount());
  set_state_bits(g_app.machine.current);

  app_wdt_init();

  taskENTER_CRITICAL();
  freertos_task_create();
  taskEXIT_CRITICAL();

  vTaskStartScheduler();
}

/*
 * Keil 工程文件尚未自动纳入新目录源码时，启用以下聚合编译入口�? * 后续�?app/domain/hal_if/bsp/cfg 加入工程后，可移除此段并独立编译�? */
#ifndef FLAMETHROWER_SEPARATE_COMPILATION
#include "../../cfg/system_config.c"
#include "../../domain/event_log.c"
#include "../../domain/machine_state.c"
#include "../../domain/fault_manager.c"
#include "../../domain/safety_guard.c"
#include "../../domain/dmx_strategy.c"
#include "../../app/app_core.c"
#include "../../bsp/at32f415/bsp_uart.c"
#include "../../bsp/at32f415/bsp_at32f415.c"
#define ST7920_COMMAND_CMD_DELAY        10
#define ST7920_COMMAND_DATA_DELAY        1
#include "../../bsp/st7920/src/driver_st7920.c"
#include "../../bsp/st7920/at32f415_st7920_port.c"
#include "../../middleware/MultiButton/multi_button.c"
#include "../../middleware/easyDMX/easy_dmx.c"
#include "../../middleware/SlateUI/core/src/sl_display.c"
#include "../../middleware/SlateUI/core/src/sl_event.c"
#include "../../middleware/SlateUI/core/src/sl_key_repeat.c"
#include "../../middleware/SlateUI/core/src/sl_language.c"
#include "../../middleware/SlateUI/core/src/sl_page_manager.c"
#include "../../middleware/SlateUI/core/src/sl_tween.c"
#include "../../middleware/SlateUI/widgets/src/sl_widget.c"
#include "../../middleware/SlateUI/widgets/src/sl_label.c"
#include "../../middleware/SlateUI/widgets/src/sl_list_view.c"
#include "../../middleware/SlateUI/widgets/src/sl_linear_layout.c"
#include "../../middleware/SlateUI/widgets/src/sl_horizontal_menu.c"
#include "../../middleware/SlateUI/widgets/src/sl_icon.c"
#include "../../middleware/SlateUI/widgets/src/sl_icon_item.c"
#include "../../middleware/SlateUI/widgets/src/sl_progress_bar.c"
#include "../../middleware/SlateUI/menu/src/sl_menu_model.c"
#include "../../middleware/SlateUI/menu/src/sl_menu_page.c"
#include "../../middleware/SlateUI/font/sl_font_ascii_16x16.c"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.c"
#include "../../middleware/SlateUI/port/sl_port.c"
#include "../../app/ui_pages/ui_idle_page.c"
#include "../../app/ui_pages/ui_main_menu.c"
#include "../../app/ui_pages/ui_safety_page.c"
#include "../../app/ui_pages/ui_setting_page.c"
#include "../../app/ui_pages/ui_language_page.c"
#include "../../app/ui_pages/ui_splash_page.c"
#include "../../app/ui_pages/ui_checking_page.c"
#endif



















