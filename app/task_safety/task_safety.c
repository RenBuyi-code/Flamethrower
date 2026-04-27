/**
 * @file    task_safety.c
 * @brief   安全任务实现
 *
 * 安全任务模块，负责：
 *   - 监控系统安全参数（压力、电压、倾斜开关等）
 *   - 检测并处理系统故障
 *   - 在紧急情况下发送安全关闭命令
 *   - 管理系统状态转换（故障、锁定等）
 *
 * 设计思路：
 *   - 定期读取传感器数据
 *   - 进行故障检测和判断
 *   - 设置或清除故障标志
 *   - 根据故障状态控制系统状态
 *   - 与其他模块的关系：
 *     - app_fsm：使用核心功能进行状态转换
 *     - task_control：通过队列发送安全关闭命令
 *     - app/rules/fault_manager：管理故障状态
 */

#include "task_safety.h"
#include "../app_task_common.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../rules/fault_manager.h"
#include <string.h>

/** @brief 安全任务全局配置（静态单例） */
static app_task_safety_cfg_t s_task_safety_cfg;

typedef struct
{
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  uint16_t voltage_raw;
  bool user_mode;
  bool tilt_fault;
  TickType_t now;
  actuator_status_t st;
  BaseType_t have_status;
  EventBits_t bits;
} task_safety_sample_t;


static void task_safety_update_fault_bits_and_log(app_fsm_t *app, uint32_t *last_fault_mask)
{
  app_task_set_fault_bits(s_task_safety_cfg.event_group, app->faults.latched_mask);
  if(*last_fault_mask != app->faults.latched_mask)
  {
    APP_LOGW("fault mask: 0x%02lX -> 0x%02lX",
             (unsigned long)(*last_fault_mask),
             (unsigned long)app->faults.latched_mask);
    *last_fault_mask = app->faults.latched_mask;
  }
}

static void task_safety_handle_fault_state_and_dmx(app_fsm_t *app, TickType_t now, bool user_mode, EventBits_t bits)
{
  (void)now;
  if((app->faults.latched_mask & FAULT_MASK_FATAL) != 0U)
  {
    app_task_send_safe_off_high_prio(s_task_safety_cfg.q_actuator);
  }
  else if((app->faults.latched_mask & FAULT_MASK_E4) != 0U)
  {
    app_task_send_safe_off_high_prio(s_task_safety_cfg.q_actuator);
  }

  if((user_mode == true) && ((bits & EVT_DMX_ONLINE_BIT) == 0U))
  {
    app_task_send_safe_off_high_prio(s_task_safety_cfg.q_actuator);
  }
}

static void task_safety_heartbeat_delay(void)
{
  (void)xEventGroupSetBits(s_task_safety_cfg.event_group, s_task_safety_cfg.hb_bit);
  vTaskDelay(pdMS_TO_TICKS(20));
}

static void task_safety_update_timeout_fault(app_fsm_t *app,
                                             fault_code_t code,
                                             bool timeout_active,
                                             bool clear_condition,
                                             TickType_t now,
                                             TickType_t *start_tick,
                                             TickType_t timeout_ticks)
{
  if(timeout_active)
  {
    if(*start_tick == 0U)
    {
      *start_tick = now;
    }
    else if((now - *start_tick) >= timeout_ticks)
    {
      fault_manager_set(&app->faults, code);
    }
  }
  else
  {
    *start_tick = 0U;
    fault_manager_try_clear(&app->faults, code, clear_condition);
  }
}

static void task_safety_check_fault_e1(app_fsm_t *app,
                                       const actuator_status_t *st,
                                       BaseType_t have_status,
                                       uint16_t pressure_raw,
                                       uint8_t pressure_pct,
                                       TickType_t now,
                                       TickType_t *e1_start)
{
  if(cfg_pressure_sensor_fault(pressure_raw))
  {
    *e1_start = 0U;
    fault_manager_set(&app->faults, FAULT_E1_PRESSURE_BUILD);
    APP_LOGW("pressure sensor fault: raw=%u", (unsigned)pressure_raw);
    return;
  }

  task_safety_update_timeout_fault(
    app,
    FAULT_E1_PRESSURE_BUILD,
    (have_status == pdTRUE) &&
      st->out.oil_pump_on &&
      (st->fire_active == false) &&
      (st->relief_active == false) &&
      (pressure_pct < CFG_PRESSURE_TARGET_PCT),
    (pressure_pct >= CFG_PRESSURE_TARGET_PCT),
    now,
    e1_start,
    pdMS_TO_TICKS(CFG_PRESSURE_ERROR_TIMEOUT_MS));
}

static void task_safety_check_fault_e3(app_fsm_t *app,
                                       uint16_t voltage_raw,
                                       TickType_t now,
                                       TickType_t *e3_start)
{
  task_safety_update_timeout_fault(app,
                                   FAULT_E3_VOLTAGE,
                                   (cfg_voltage_raw_in_range(voltage_raw) == false),
                                   true,
                                   now,
                                   e3_start,
                                   pdMS_TO_TICKS(CFG_VOLTAGE_ERROR_HOLD_MS));
}

static void task_safety_check_fault_e5(app_fsm_t *app,
                                       const actuator_status_t *st,
                                       BaseType_t have_status,
                                       uint8_t pressure_pct,
                                       TickType_t now,
                                       TickType_t *e5_start)
{
  task_safety_update_timeout_fault(app,
                                   FAULT_E5_RELIEF,
                                   (have_status == pdTRUE) &&
                                     st->out.relief_valve_on &&
                                     (pressure_pct > CFG_PRESSURE_RELIEF_DONE_PCT),
                                   (pressure_pct <= CFG_PRESSURE_RELIEF_DONE_PCT),
                                   now,
                                   e5_start,
                                   pdMS_TO_TICKS(CFG_RELIEF_ERROR_TIMEOUT_MS));
}

static void task_safety_check_fault_e2(app_fsm_t *app, bool tilt_fault)
{
  if(app->params.tilt_protect_enable && tilt_fault)
  {
    fault_manager_set(&app->faults, FAULT_E2_TILT);
  }
  else
  {
    fault_manager_try_clear(&app->faults, FAULT_E2_TILT, true);
  }
}

static void task_safety_check_fault_e4(app_fsm_t *app, bool user_mode)
{
  if((user_mode == false) && (app->machine.current != MACHINE_SELFTEST))
  {
    fault_manager_set(&app->faults, FAULT_E4_LOCKED_MODE);
  }
  else
  {
    fault_manager_try_clear(&app->faults, FAULT_E4_LOCKED_MODE, true);
  }
}

static void task_safety_collect_sample(app_fsm_t *app, task_safety_sample_t *sample)
{
  sample->now = xTaskGetTickCount();
  sample->pressure_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_PRESSURE);
  sample->pressure_pct = cfg_pressure_raw_to_percent(sample->pressure_raw);
  sample->voltage_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_POWER1);
  sample->user_mode = app->hal.input.read(app->hal.input.ctx, INPUT_SAFETY_LOCK);
  sample->tilt_fault = app->hal.input.read(app->hal.input.ctx, INPUT_TILT_SWITCH);
  sample->have_status = xQueuePeek(s_task_safety_cfg.q_actuator_status, &sample->st, 0);
  sample->bits = xEventGroupGetBits(s_task_safety_cfg.event_group);
}

static void task_safety_run_fault_checks(app_fsm_t *app,
                                         const task_safety_sample_t *sample,
                                         TickType_t *e1_start,
                                         TickType_t *e3_start,
                                         TickType_t *e5_start)
{
  task_safety_check_fault_e1(app,
                             &sample->st,
                             sample->have_status,
                             sample->pressure_raw,
                             sample->pressure_pct,
                             sample->now,
                             e1_start);
  task_safety_check_fault_e2(app, sample->tilt_fault);
  task_safety_check_fault_e3(app, sample->voltage_raw, sample->now, e3_start);
  task_safety_check_fault_e4(app, sample->user_mode);
  task_safety_check_fault_e5(app, &sample->st, sample->have_status, sample->pressure_pct, sample->now, e5_start);
}

/**
 * @brief   Initialize safety task configuration.
 *
 * @param[in] cfg Safety task configuration pointer.
 *                When NULL, clear static configuration for recovery.
 */
void app_task_safety_init(const app_task_safety_cfg_t *cfg)
{
  if(cfg == 0)
  {
    memset(&s_task_safety_cfg, 0, sizeof(s_task_safety_cfg));
    return;
  }

  s_task_safety_cfg = *cfg;
}

/**
 * @brief   安全任务主体
 *
 * @param[in] pvParameters 未使用（标准 FreeRTOS 接口）
 *
 * 主循环逻辑：
 *   1. 读取传感器数据（压力、电压、倾斜开关等）
 *   2. 检测各种故障条件
 *   3. 设置或清除故障标志
 *   4. 根据故障状态控制系统状态
 *   5. 定期发送心跳标志
 *
 * 故障检测类型：
 *   - FAULT_E1_PRESSURE_BUILD：压力建立失败
 *   - FAULT_E2_TILT：倾斜保护触发
 *   - FAULT_E3_VOLTAGE：电压异常
 *   - FAULT_E4_LOCKED_MODE：安全锁未解锁
 *   - FAULT_E5_RELIEF：泄压失败
 */
void safety_task(void *pvParameters)
{
  TickType_t e1_start;       /**< E1故障计时开始时间 */
  TickType_t e3_start;       /**< E3故障计时开始时间 */
  TickType_t e5_start;       /**< E5故障计时开始时间 */
  uint32_t last_fault_mask;  /**< 上一次故障掩码 */
  task_safety_sample_t sample;/**< 本轮采样快照 */
  app_fsm_t *app;           /**< 应用核心实例 */
  (void)pvParameters;

  e1_start = 0U;
  e3_start = 0U;
  e5_start = 0U;
  last_fault_mask = 0U;

  for(;;)
  {
    app = s_task_safety_cfg.app;
    if((app == 0) || (s_task_safety_cfg.event_group == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    task_safety_collect_sample(app, &sample);

    /**
     * 故障检测 E1：压力建立失败
     * 条件：
     *   1. 压力传感器故障
     *   2. 油泵开启但压力未达到目标值（超时）
     */
    task_safety_run_fault_checks(app, &sample, &e1_start, &e3_start, &e5_start);

    /**
     * 更新故障事件标志
     * 当故障掩码变化时，输出日志
     */
    task_safety_update_fault_bits_and_log(app, &last_fault_mask);

    /**
     * 处理致命故障：发送安全关闭命令，切换到故障状态
     */
    task_safety_handle_fault_state_and_dmx(app, sample.now, sample.user_mode, sample.bits);

    /**
     * 设置心跳标志，通知其他任务安全任务正常运行
     */
    task_safety_heartbeat_delay();
  }
}
