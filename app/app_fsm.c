/**
 * @file    app_fsm.c
 * @brief   应用核心实现
 *
 * 应用核心模块，负责：
 *   - 初始化系统各组件（HAL、状态机、故障管理、事件日志等）
 *   - 管理系统参数（加载/默认值）
 *   - 处理状态转换
 *   - 记录系统事件
 *
 * 设计思路：
 *   - 保持核心逻辑简洁，将业务规则放在 app/rules/ 目录
 *   - 集中管理系统状态，确保单一事实来源
 *   - 提供统一的日志和状态转换接口
 *   - 与其他模块的关系：
 *     - bsp/：硬件抽象层，提供底层硬件访问
 *     - app/rules/：领域模型，包含业务规则
 *     - app/task_*：任务模块，使用核心功能
 */

#include "app_fsm.h"
#include "log_rtt.h"

/**
 * @brief   初始化应用核心
 *
 * @param[in] core  应用核心实例指针
 *
 * 初始化流程：
 *   1. 绑定硬件抽象层（HAL）
 *   2. 初始化机器状态机
 *   3. 初始化故障管理器
 *   4. 初始化事件日志
 *   5. 加载默认参数
 *
 * @note    此函数应在系统启动时调用
 */
void app_fsm_init(app_fsm_t *core)
{
  if(core == 0)
  {
    return;
  }

  bsp_at32f415_bind(&core->hal);
  state_machine_init(&core->machine);
  fault_manager_init(&core->faults);
  event_log_init(&core->events);
  cfg_get_default_params(&core->params);
}

/**
 * @brief   加载参数或使用默认值
 *
 * @param[in] core  应用核心实例指针
 *
 * 操作流程：
 *   1. 尝试从存储中加载参数
 *   2. 如果加载失败，使用默认参数
 *   3. 对参数进行校验和修正
 *   4. 记录参数信息到日志
 *
 * @note    此函数应在初始化后调用，确保参数正确加载
 */
void app_fsm_load_or_default_params(app_fsm_t *core)
{
  if(core == 0)
  {
    return;
  }

  if(core->hal.storage.load_params(core->hal.storage.ctx, &core->params) == false)
  {
    cfg_get_default_params(&core->params);
    APP_LOGW("params load failed, use defaults");
  }

  cfg_sanitize_params(&core->params);
  APP_LOGI("params: addr=%u mode=%u ign=%u lock=%u tilt=%u",
           (unsigned)core->params.dmx_address,
           (unsigned)core->params.dmx_mode,
           (unsigned)core->params.igniter_delay_sec,
           (unsigned)core->params.oil_lock_delay_sec,
           (unsigned)core->params.tilt_protect_enable);
}

bool app_fsm_apply_params(app_fsm_t *core, const system_params_t *params)
{
  if((core == 0) || (params == 0))
  {
    return false;
  }

  core->params = *params;
  cfg_sanitize_params(&core->params);
  if(core->hal.storage.save_params(core->hal.storage.ctx, &core->params) == false)
  {
    APP_LOGW("params save failed");
    return false;
  }

  APP_LOGI("params applied: addr=%u mode=%u ign=%u lock=%u tilt=%u",
           (unsigned)core->params.dmx_address,
           (unsigned)core->params.dmx_mode,
           (unsigned)core->params.igniter_delay_sec,
           (unsigned)core->params.oil_lock_delay_sec,
           (unsigned)core->params.tilt_protect_enable);
  return true;
}

bool app_fsm_get_params_snapshot(const app_fsm_t *core, system_params_t *out)
{
  if((core == 0) || (out == 0))
  {
    return false;
  }

  *out = core->params;
  return true;
}

/**
 * @brief   记录系统事件
 *
 * @param[in] core     应用核心实例指针
 * @param[in] code     事件代码
 * @param[in] ts_ms    事件时间戳（毫秒）
 *
 * @note    此函数将事件推入事件日志队列
 */
void app_fsm_log(app_fsm_t *core, uint16_t code, uint32_t ts_ms)
{
  if(core == 0)
  {
    return;
  }

  event_log_push(&core->events, code, ts_ms);
}

/**
 * @brief   切换机器状态
 *
 * @param[in] core       应用核心实例指针
 * @param[in] next       目标状态
 * @param[in] event_code 触发事件代码
 * @param[in] ts_ms      事件时间戳（毫秒）
 * @return    状态转换是否成功
 *
 * 状态转换流程：
 *   1. 检查参数合法性
 *   2. 检查是否为相同状态（避免冗余转换）
 *   3. 执行状态转换
 *   4. 记录状态转换事件
 *   5. 输出状态转换日志
 *
 * @note    状态转换失败时会记录错误事件
 */
bool app_fsm_transition(app_fsm_t *core, machine_state_t next, uint16_t event_code, uint32_t ts_ms)
{
  bool ok;
  machine_state_t from;

  if(core == 0)
  {
    return false;
  }

  from = core->machine.current;
  if(from == next)
  {
    return true;
  }

  ok = state_machine_transition(&core->machine, next, event_code);
  if(ok)
  {
    app_fsm_log(core, event_code, ts_ms);
    APP_LOGI("state %u -> %u ev=0x%04X",
             (unsigned)from,
             (unsigned)next,
             (unsigned)event_code);
  }
  else
  {
    app_fsm_log(core, (uint16_t)(0xF000U | event_code), ts_ms);
    APP_LOGW("illegal transition state=%u to=%u ev=0x%04X",
             (unsigned)from,
             (unsigned)next,
             (unsigned)event_code);
  }

  return ok;
}
