/**
 * @file freertos_app.c
 * @brief RTOS startup and wiring.
 */

#include "freertos_app.h"
#include "../inc/at32f415_conf.h"
#include "../../app/app_fsm.h"
#include "../../app/app_selftest.h"
#include "../../app/app_task_common.h"
#include "../../app/app_task_shared.h"
#include "../../app/task_safety/task_safety.h"
#include "../../app/task_control/task_control.h"
#include "../../app/task_ui/task_ui.h"
#include "../../app/task_dmx/task_dmx.h"
#include "../../app/task_actuator/task_actuator.h"
#include "../../app/task_diag/task_diag.h"
#include "../../cfg/system_config.h"
#include "../../middleware/easyDMX/easy_dmx.h"

#define APP_WDT_ENABLE            0U
#define APP_WDT_DIVIDER           WDT_CLK_DIV_256
#define APP_WDT_RELOAD            1000U

TaskHandle_t safety_handle;
TaskHandle_t control_handle;
TaskHandle_t actuator_handle;
TaskHandle_t dmx_handle;
TaskHandle_t ui_handle;
TaskHandle_t diag_handle;

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

static app_fsm_t g_app;

static edmx_rx_t s_dmx_rx;
static uint8_t s_dmx_fifo_storage[2048];

static uint32_t g_ui_perf_last_cycles;
static uint32_t g_ui_perf_monotonic_us;
static uint32_t g_ui_perf_cycles_per_us;
static bool g_ui_perf_dwt_ready;

static bool g_ui_menu_active;

static void app_wdt_feed(void);

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

static void app_init_runtime_objects(void)
{
  q_actuator = xQueueCreateStatic(8, sizeof(actuator_cmd_t), actuator_queue_storage, &actuator_queue_tcb);
  q_actuator_status = xQueueCreateStatic(1, sizeof(actuator_status_t), actuator_status_storage, &actuator_status_queue_tcb);
  eg_system = xEventGroupCreateStatic(&evt_group_tcb);
  (void)edmx_rx_init(&s_dmx_rx, s_dmx_fifo_storage, sizeof(s_dmx_fifo_storage), CFG_DMX_LOST_TIMEOUT_MS);
}

static void app_enter_selftest_state(void)
{
  (void)app_fsm_transition(&g_app, MACHINE_SELFTEST, 0x2001U, (uint32_t)xTaskGetTickCount());
  app_task_set_state_bits(eg_system, g_app.machine.current);
}

static void app_wdt_feed(void)
{
#if (APP_WDT_ENABLE != 0U)
  wdt_counter_reload();
#endif
}

static void app_init_task_configs(void)
{
  app_task_dmx_cfg_t dmx_cfg;
  app_task_safety_cfg_t safety_cfg;
  app_task_control_cfg_t control_cfg;
  app_task_actuator_cfg_t actuator_cfg;
  app_task_diag_cfg_t diag_cfg;
  app_task_ui_cfg_t ui_cfg;

  dmx_cfg.app = &g_app;
  dmx_cfg.rx = &s_dmx_rx;
  dmx_cfg.event_group = eg_system;
  dmx_cfg.hb_bit = EVT_HB_DMX_BIT;
  app_task_dmx_init(&dmx_cfg);

  safety_cfg.app = &g_app;
  safety_cfg.q_actuator = q_actuator;
  safety_cfg.q_actuator_status = q_actuator_status;
  safety_cfg.event_group = eg_system;
  safety_cfg.hb_bit = EVT_HB_SAFETY_BIT;
  app_task_safety_init(&safety_cfg);

  control_cfg.app = &g_app;
  control_cfg.dmx_rx = &s_dmx_rx;
  control_cfg.q_actuator = q_actuator;
  control_cfg.event_group = eg_system;
  control_cfg.hb_bit = EVT_HB_CONTROL_BIT;
  control_cfg.ui_menu_active = &g_ui_menu_active;
  app_task_control_init(&control_cfg);

  actuator_cfg.app = &g_app;
  actuator_cfg.q_actuator = q_actuator;
  actuator_cfg.q_actuator_status = q_actuator_status;
  actuator_cfg.event_group = eg_system;
  actuator_cfg.hb_bit = EVT_HB_ACTUATOR_BIT;
  actuator_cfg.led_error_mask = EVT_STATE_FAULT_BIT | EVT_FAULT_E1_BIT | EVT_FAULT_E2_BIT | EVT_FAULT_E3_BIT | EVT_FAULT_E5_BIT;
  actuator_cfg.dmx_online_bit = EVT_DMX_ONLINE_BIT;
  app_task_actuator_init(&actuator_cfg);

  diag_cfg.q_actuator = q_actuator;
  diag_cfg.event_group = eg_system;
  diag_cfg.hb_mask = EVT_HB_MASK;
  diag_cfg.hb_bit = EVT_HB_DIAG_BIT;
  diag_cfg.miss_timeout_ms = 1000U;
  diag_cfg.loop_delay_ms = 200U;
  diag_cfg.wdt_feed = app_wdt_feed;
  app_task_diag_init(&diag_cfg);

  ui_cfg.app = &g_app;
  ui_cfg.q_actuator_status = q_actuator_status;
  ui_cfg.event_group = eg_system;
  ui_cfg.dmx_online_bit = EVT_DMX_ONLINE_BIT;
  ui_cfg.hb_bit = EVT_HB_UI_BIT;
  ui_cfg.menu_active = &g_ui_menu_active;
  app_task_ui_init(&ui_cfg);
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
  app_fsm_init(&g_app);
  app_fsm_load_or_default_params(&g_app);
  app_run_rules_selftests();

  app_init_runtime_objects();
  app_init_task_configs();
  app_enter_selftest_state();

  app_wdt_init();

  taskENTER_CRITICAL();
  freertos_task_create();
  taskEXIT_CRITICAL();

  vTaskStartScheduler();
}
