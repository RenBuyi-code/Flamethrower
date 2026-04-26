/**
 * @file    freertos_app.c
 * @brief   RTOS应用主文件
 *
 * RTOS应用核心模块，负责：
 *   - 系统初始化（硬件、软件）
 *   - FreeRTOS任务创建
 *   - 任务间通信机制（队列、事件组）
 *   - 执行器任务实现
 *   - 诊断任务（心跳监控）
 *   - 领域自检测试
 *
 * 系统架构：
 *   - 6个主要任务：safety、control、actuator、dmx、ui、diag
 *   - 2个队列：actuator_cmd（命令）、actuator_status（状态）
 *   - 1个事件组：系统事件和心跳标志
 *
 * 设计思路：
 *   - 安全优先架构
 *   - 静态任务和队列创建，减少运行时开销
 *   - 心跳机制监控任务健康状态
 *   - 与其他模块的关系：
 *     - app/task_*：使用任务模块
 *     - domain：进行领域自检
 */

#include "freertos_app.h"
#include "../inc/at32f415_conf.h"
#include "../../app/app_core.h"
#include "../../app/app_task_common.h"
#include "../../app/app_task_shared.h"
#include "../../app/ui_services.h"
#include "../../app/task_safety/task_safety.h"
#include "../../app/task_control/task_control.h"
#include "../../app/task_ui/task_ui.h"
#include "../../app/task_dmx/task_dmx.h"
#include "../../app/log_rtt.h"
#include "../../cfg/system_config.h"
#include "../../domain/dmx_strategy.h"
#include "../../domain/fault_manager.h"
#include "../../domain/safety_guard.h"
#include "../../middleware/easyDMX/easy_dmx.h"
#include <string.h>

/** @brief 看门狗使能开关（调试阶段关闭） */
#define APP_WDT_ENABLE                    0U
/** @brief 看门狗时钟分频 */
#define APP_WDT_DIVIDER                   WDT_CLK_DIV_256
/** @brief 看门狗重载值 */
#define APP_WDT_RELOAD                    1000U
/** @brief 领域自检使能开关 */
#define APP_DOMAIN_SELFTEST_ENABLE        1U

/**
 * @brief   任务句柄定义
 *
 * 用于引用和控制各任务
 */
TaskHandle_t safety_handle;     /**< 安全任务句柄 */
TaskHandle_t control_handle;     /**< 控制任务句柄 */
TaskHandle_t actuator_handle;    /**< 执行器任务句柄 */
TaskHandle_t dmx_handle;        /**< DMX任务句柄 */
TaskHandle_t ui_handle;         /**< UI任务句柄 */
TaskHandle_t diag_handle;       /**< 诊断任务句柄 */

/**
 * @brief   空闲任务内存
 *
 * 静态分配的空闲任务控制块和栈
 */
static StackType_t idle_task_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t idle_task_tcb;

/**
 * @brief   任务控制块和栈（静态分配）
 *
 * 为每个任务预分配控制块和栈内存
 */
static StaticTask_t safety_tcb;
static StaticTask_t control_tcb;
static StaticTask_t actuator_tcb;
static StaticTask_t dmx_tcb;
static StaticTask_t ui_tcb;
static StaticTask_t diag_tcb;

/**
 * @brief   任务栈大小定义
 *
 * 每个任务的栈大小（单位：字）
 */
static StackType_t safety_stack[320];
static StackType_t control_stack[384];
static StackType_t actuator_stack[320];
static StackType_t dmx_stack[256];
static StackType_t ui_stack[256];
static StackType_t diag_stack[256];

/**
 * @brief   执行器命令队列
 *
 * 存储待执行的执行器命令
 */
static StaticQueue_t actuator_queue_tcb;
static uint8_t actuator_queue_storage[8 * sizeof(actuator_cmd_t)];

/**
 * @brief   执行器状态队列
 *
 * 存储当前执行器状态（单消费者）
 */
static StaticQueue_t actuator_status_queue_tcb;
static uint8_t actuator_status_storage[sizeof(actuator_status_t)];

/**
 * @brief   系统事件组
 *
 * 用于任务间同步和事件标志管理
 */
static StaticEventGroup_t evt_group_tcb;

/**
 * @brief   队列和事件组句柄
 */
static QueueHandle_t q_actuator;            /**< 执行器命令队列 */
static QueueHandle_t q_actuator_status;     /**< 执行器状态队列 */
static EventGroupHandle_t eg_system;         /**< 系统事件组 */

/**
 * @brief   全局应用核心实例
 */
static app_core_t g_app;

/**
 * @brief   DMX接收器实例
 */
static edmx_rx_t s_dmx_rx;
static uint8_t s_dmx_fifo_storage[1024];

/**
 * @brief   UI性能监控变量
 */
static uint32_t g_ui_perf_last_cycles;
static uint32_t g_ui_perf_monotonic_us;
static uint32_t g_ui_perf_cycles_per_us;
static bool g_ui_perf_dwt_ready;

/**
 * @brief   UI菜单活跃状态
 */
static bool g_ui_menu_active;

/**
 * @brief   提交参数修改
 *
 * 对参数进行校验后保存到Flash
 */
static void app_params_commit(void);

/**
 * @brief   获取空闲任务内存（静态分配）
 *
 * @param[out] ppxIdleTaskTCBBuffer     空闲任务TCB缓冲区
 * @param[out] ppxIdleTaskStackBuffer   空闲任务栈缓冲区
 * @param[out] pulIdleTaskStackSize     空闲任务栈大小
 */
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

/**
 * @brief   初始化UI性能监控
 *
 * 配置CPU周期计数器用于性能测量
 */
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

/**
 * @brief   获取当前微秒数
 *
 * @return    微秒数（DWT计数器或系统tick）
 */
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

/**
 * @brief   初始化看门狗
 *
 * 配置并使能看门狗（调试阶段默认关闭）
 */
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

/**
 * @brief   喂狗
 *
 * 重置看门狗计数器，防止系统复位
 */
static void app_wdt_feed(void)
{
#if (APP_WDT_ENABLE != 0U)
  wdt_counter_reload();
#endif
}

/**
 * @brief   提交参数修改
 *
 * 校验参数后保存到Flash存储
 */
static void app_params_commit(void)
{
  cfg_sanitize_params(&g_app.params);
  (void)g_app.hal.storage.save_params(g_app.hal.storage.ctx, &g_app.params);
}

/**
 * @brief   自检：机器状态机
 *
 * @return    测试是否通过
 *
 * 测试状态转换逻辑，包括合法转换和非法转换
 */
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

/**
 * @brief   自检：DMX策略
 *
 * @return    测试是否通过
 *
 * 测试DMX解析逻辑，包括2CH和6CH模式
 */
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

/**
 * @brief   自检：故障管理器
 *
 * @return    测试是否通过
 *
 * 测试故障设置、清除和查询逻辑
 */
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

/**
 * @brief   自检：安全防护
 *
 * @return    测试是否通过
 *
 * 测试安全评估逻辑，包括各种安全动作
 */
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

/**
 * @brief   自检：EasyDMX
 *
 * @return    测试是否通过
 *
 * 测试DMX接收器逻辑，包括Break检测和帧解析
 */
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

/**
 * @brief   运行领域自检
 *
 * 执行所有领域模块的自检测试
 */
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

/**
 * @brief   执行器任务
 *
 * @param[in] pvParameters  未使用
 *
 * 职责：
 *   - 接收执行器命令
 *   - 管理执行器状态（油泵、阀门、点火器等）
 *   - 处理点火时序
 *   - 控制LED指示灯
 *   - 更新执行器状态供其他任务查询
 */
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

/**
 * @brief   诊断任务
 *
 * @param[in] pvParameters  未使用
 *
 * 职责：
 *   - 监控所有任务的心跳标志
 *   - 检测任务是否停止响应
 *   - 在任务失败时发送安全关闭命令
 *   - 定期喂狗
 */
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
        app_task_queue_send_latest(q_actuator, &cmd, pdTRUE);
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

/**
 * @brief   创建所有FreeRTOS任务
 *
 * 使用静态分配创建所有任务
 */
void freertos_task_create(void)
{
  safety_handle = xTaskCreateStatic(safety_task, "safety", 320, 0, configMAX_PRIORITIES - 1U, safety_stack, &safety_tcb);
  control_handle = xTaskCreateStatic(control_task, "control", 384, 0, configMAX_PRIORITIES - 3U, control_stack, &control_tcb);
  actuator_handle = xTaskCreateStatic(actuator_task, "actuator", 320, 0, configMAX_PRIORITIES - 2U, actuator_stack, &actuator_tcb);
  dmx_handle = xTaskCreateStatic(dmx_task, "dmx", 256, 0, configMAX_PRIORITIES - 4U, dmx_stack, &dmx_tcb);
  ui_handle = xTaskCreateStatic(ui_task, "ui", 256, 0, configMAX_PRIORITIES - 6U, ui_stack, &ui_tcb);
  diag_handle = xTaskCreateStatic(diag_task, "diag", 256, 0, configMAX_PRIORITIES - 5U, diag_stack, &diag_tcb);
}

/**
 * @brief   FreeRTOS初始化入口
 *
 * 初始化流程：
 *   1. 初始化UI性能监控
 *   2. 初始化应用核心
 *   3. 加载或默认参数
 *   4. 运行领域自检
 *   5. 创建队列和事件组
 *   6. 初始化DMX接收器
 *   7. 初始化所有任务配置
 *   8. 切换到自检状态
 *   9. 初始化看门狗
 *   10. 创建所有任务
 *   11. 启动调度器
 */
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

  {
    app_task_dmx_cfg_t dmx_cfg;
    app_task_safety_cfg_t safety_cfg;
    app_task_control_cfg_t control_cfg;
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

    ui_cfg.app = &g_app;
    ui_cfg.q_actuator_status = q_actuator_status;
    ui_cfg.event_group = eg_system;
    ui_cfg.dmx_online_bit = EVT_DMX_ONLINE_BIT;
    ui_cfg.hb_bit = EVT_HB_UI_BIT;
    ui_cfg.menu_active = &g_ui_menu_active;
    ui_cfg.commit_params = app_params_commit;
    app_task_ui_init(&ui_cfg);
  }

  (void)app_core_switch_state(&g_app, MACHINE_SELFTEST, 0x2001U, (uint32_t)xTaskGetTickCount());
  app_task_set_state_bits(eg_system, g_app.machine.current);

  app_wdt_init();

  taskENTER_CRITICAL();
  freertos_task_create();
  taskEXIT_CRITICAL();

  vTaskStartScheduler();
}
