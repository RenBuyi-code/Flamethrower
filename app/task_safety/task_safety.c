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
 *     - app_core：使用核心功能进行状态转换
 *     - task_control：通过队列发送安全关闭命令
 *     - domain/fault_manager：管理故障状态
 */

#include "task_safety.h"
#include "../app_task_common.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../../domain/fault_manager.h"
#include <string.h>

/** @brief 安全任务全局配置（静态单例） */
static app_task_safety_cfg_t s_task_safety_cfg;

/**
 * @brief   初始化安全任务配置
 *
 * @param[in] cfg  安全任务配置结构体指针
 *
 * 当 cfg == NULL 时，清除配置（用于异常恢复）
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
  uint16_t pressure_raw;    /**< 压力传感器原始值 */
  uint8_t pressure_pct;      /**< 压力百分比值 */
  uint16_t voltage_raw;      /**< 电压传感器原始值 */
  bool user_mode;            /**< 用户模式（安全锁状态） */
  bool tilt_fault;           /**< 倾斜故障状态 */
  TickType_t now;            /**< 当前系统时间 */
  TickType_t e1_start;       /**< E1故障计时开始时间 */
  TickType_t e3_start;       /**< E3故障计时开始时间 */
  TickType_t e5_start;       /**< E5故障计时开始时间 */
  uint32_t last_fault_mask;  /**< 上一次故障掩码 */
  actuator_status_t st;      /**< 执行器状态 */
  BaseType_t have_status;     /**< 是否获取到执行器状态 */
  EventBits_t bits;           /**< 事件标志位 */
  app_core_t *app;           /**< 应用核心实例 */
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

    now = xTaskGetTickCount();
    pressure_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    voltage_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_POWER1);
    user_mode = app->hal.input.read(app->hal.input.ctx, INPUT_SAFETY_LOCK);
    tilt_fault = app->hal.input.read(app->hal.input.ctx, INPUT_TILT_SWITCH);

    have_status = xQueuePeek(s_task_safety_cfg.q_actuator_status, &st, 0);
    bits = xEventGroupGetBits(s_task_safety_cfg.event_group);

    /**
     * 故障检测 E1：压力建立失败
     * 条件：
     *   1. 压力传感器故障
     *   2. 油泵开启但压力未达到目标值（超时）
     */
    if(cfg_pressure_sensor_fault(pressure_raw))
    {
      e1_start = 0U;
      fault_manager_set(&app->faults, FAULT_E1_PRESSURE_BUILD);
      APP_LOGW("pressure sensor fault: raw=%u", (unsigned)pressure_raw);
    }
    else if((have_status == pdTRUE) &&
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
        fault_manager_set(&app->faults, FAULT_E1_PRESSURE_BUILD);
      }
    }
    else
    {
      e1_start = 0U;
      fault_manager_try_clear(&app->faults, FAULT_E1_PRESSURE_BUILD,
                              (cfg_pressure_sensor_fault(pressure_raw) == false) &&
                              (pressure_pct >= CFG_PRESSURE_TARGET_PCT));
    }

    /**
     * 故障检测 E2：倾斜保护
     * 条件：
     *   1. 倾斜保护功能启用
     *   2. 倾斜开关触发
     */
    if(app->params.tilt_protect_enable && tilt_fault)
    {
      fault_manager_set(&app->faults, FAULT_E2_TILT);
    }
    else
    {
      fault_manager_try_clear(&app->faults, FAULT_E2_TILT, true);
    }

    /**
     * 故障检测 E3：电压异常
     * 条件：
     *   1. 电压值不在正常范围内
     *   2. 持续时间超过阈值
     */
    if(cfg_voltage_raw_in_range(voltage_raw) == false)
    {
      if(e3_start == 0U)
      {
        e3_start = now;
      }
      else if((now - e3_start) >= pdMS_TO_TICKS(CFG_VOLTAGE_ERROR_HOLD_MS))
      {
        fault_manager_set(&app->faults, FAULT_E3_VOLTAGE);
      }
    }
    else
    {
      e3_start = 0U;
      fault_manager_try_clear(&app->faults, FAULT_E3_VOLTAGE, true);
    }

    /**
     * 故障检测 E4：安全锁未解锁
     * 条件：
     *   1. 安全锁未解锁（非用户模式）
     *   2. 系统不在自检状态
     */
    if((user_mode == false) && (app->machine.current != MACHINE_SELFTEST))
    {
      fault_manager_set(&app->faults, FAULT_E4_LOCKED_MODE);
    }
    else
    {
      fault_manager_try_clear(&app->faults, FAULT_E4_LOCKED_MODE, true);
    }

    /**
     * 故障检测 E5：泄压失败
     * 条件：
     *   1. 泄压阀开启
     *   2. 压力未降到目标值
     *   3. 持续时间超过阈值
     */
    if((have_status == pdTRUE) && st.out.relief_valve_on && (pressure_pct > CFG_PRESSURE_RELIEF_DONE_PCT))
    {
      if(e5_start == 0U)
      {
        e5_start = now;
      }
      else if((now - e5_start) >= pdMS_TO_TICKS(CFG_RELIEF_ERROR_TIMEOUT_MS))
      {
        fault_manager_set(&app->faults, FAULT_E5_RELIEF);
      }
    }
    else
    {
      e5_start = 0U;
      fault_manager_try_clear(&app->faults, FAULT_E5_RELIEF, pressure_pct <= CFG_PRESSURE_RELIEF_DONE_PCT);
    }

    /**
     * 更新故障事件标志
     * 当故障掩码变化时，输出日志
     */
    app_task_set_fault_bits(s_task_safety_cfg.event_group, app->faults.latched_mask);
    if(last_fault_mask != app->faults.latched_mask)
    {
      APP_LOGW("fault mask: 0x%02lX -> 0x%02lX",
               (unsigned long)last_fault_mask,
               (unsigned long)app->faults.latched_mask);
      last_fault_mask = app->faults.latched_mask;
    }

    /**
     * 处理致命故障：发送安全关闭命令，切换到故障状态
     */
    if((app->faults.latched_mask & FAULT_MASK_FATAL) != 0U)
    {
      app_task_send_safe_off_high_prio(s_task_safety_cfg.q_actuator);
      (void)app_core_switch_state(app, MACHINE_FAULT, 0x2101U, (uint32_t)now);
      app_task_set_state_bits(s_task_safety_cfg.event_group, app->machine.current);
    }
    /**
     * 处理锁定故障：切换到锁定状态
     */
    else if((app->faults.latched_mask & FAULT_MASK_E4) != 0U)
    {
      (void)app_core_switch_state(app, MACHINE_LOCKED, 0x2102U, (uint32_t)now);
      app_task_set_state_bits(s_task_safety_cfg.event_group, app->machine.current);
    }

    /**
     * DMX 离线处理：当用户模式且DMX离线时，发送安全关闭命令
     */
    if((user_mode == true) && ((bits & EVT_DMX_ONLINE_BIT) == 0U))
    {
      app_task_send_safe_off_high_prio(s_task_safety_cfg.q_actuator);
    }

    /**
     * 设置心跳标志，通知其他任务安全任务正常运行
     */
    (void)xEventGroupSetBits(s_task_safety_cfg.event_group, s_task_safety_cfg.hb_bit);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
