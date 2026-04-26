/**
 * @file    machine_state.c
 * @brief   机器状态管理实现
 *
 * 机器状态管理模块，负责：
 *   - 管理机器的状态转换
 *   - 确保状态转换的合法性
 *   - 记录状态转换历史
 *
 * 状态机设计：
 *   - 6个状态：启动、自检、就绪、点火、泄压、故障、锁定
 *   - 严格的状态转换规则，防止非法转换
 *   - 记录每次转换的事件代码
 *
 * 设计思路：
 *   - 使用状态机模式管理机器生命周期
 *   - 通过can_move函数定义状态转换规则
 *   - 与其他模块的关系：
 *     - app_core：使用状态机进行状态转换
 *     - task_control：响应状态变化执行相应操作
 */

#include "machine_state.h"

/**
 * @brief   检查状态转换是否合法
 *
 * @param[in] from  源状态
 * @param[in] to    目标状态
 * @return    是否可以转换
 *
 * 状态转换规则：
 *   - BOOT：只能转到SELFTEST
 *   - SELFTEST：可转到READY、FAULT、LOCKED
 *   - READY：可转到SELFTEST、FIRING、RELIEF、FAULT、LOCKED
 *   - FIRING：可转到READY、RELIEF、FAULT、LOCKED、SELFTEST
 *   - RELIEF：可转到READY、FAULT、LOCKED、SELFTEST
 *   - FAULT：可转到READY、LOCKED、SELFTEST
 *   - LOCKED：可转到READY、FAULT、SELFTEST
 */
static bool can_move(machine_state_t from, machine_state_t to)
{
  switch(from)
  {
    case MACHINE_BOOT:
      return (to == MACHINE_SELFTEST);
    case MACHINE_SELFTEST:
      return (to == MACHINE_READY) || (to == MACHINE_FAULT) || (to == MACHINE_LOCKED);
    case MACHINE_READY:
      return (to == MACHINE_SELFTEST) || (to == MACHINE_FIRING) || (to == MACHINE_RELIEF) || (to == MACHINE_FAULT) || (to == MACHINE_LOCKED);
    case MACHINE_FIRING:
      return (to == MACHINE_READY) || (to == MACHINE_RELIEF) || (to == MACHINE_FAULT) || (to == MACHINE_LOCKED) || (to == MACHINE_SELFTEST);
    case MACHINE_RELIEF:
      return (to == MACHINE_READY) || (to == MACHINE_FAULT) || (to == MACHINE_LOCKED) || (to == MACHINE_SELFTEST);
    case MACHINE_FAULT:
      return (to == MACHINE_READY) || (to == MACHINE_LOCKED) || (to == MACHINE_SELFTEST);
    case MACHINE_LOCKED:
      return (to == MACHINE_READY) || (to == MACHINE_FAULT) || (to == MACHINE_SELFTEST);
    default:
      return false;
  }
}

/**
 * @brief   初始化机器状态
 *
 * @param[out] ctx  机器状态上下文指针
 *
 * 初始化操作：
 *   1. 设置初始状态为BOOT
 *   2. 清零最后事件代码
 *   3. 清零转换计数
 */
void machine_state_init(machine_state_ctx_t *ctx)
{
  if(ctx == 0)
  {
    return;
  }
  ctx->current = MACHINE_BOOT;
  ctx->last_event = 0U;
  ctx->transition_count = 0U;
}

/**
 * @brief   执行状态转换
 *
 * @param[out] ctx         机器状态上下文指针
 * @param[in] to           目标状态
 * @param[in] event_code   触发事件代码
 * @return    是否转换成功
 *
 * 操作流程：
 *   1. 检查参数有效性
 *   2. 检查状态转换是否合法
 *   3. 更新当前状态
 *   4. 记录事件代码
 *   5. 增加转换计数
 */
bool machine_state_transition(machine_state_ctx_t *ctx, machine_state_t to, uint16_t event_code)
{
  if(ctx == 0)
  {
    return false;
  }

  if(can_move(ctx->current, to) == false)
  {
    return false;
  }

  ctx->current = to;
  ctx->last_event = event_code;
  ctx->transition_count++;
  return true;
}
