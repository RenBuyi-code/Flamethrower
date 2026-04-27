/**
 * @file    fault_manager.c
 * @brief   故障管理实现
 *
 * 故障管理模块，负责：
 *   - 管理故障状态（设置、清除、查询）
 *   - 使用锁存机制防止故障状态抖动
 *   - 区分致命故障和非致命故障
 *
 * 设计思路：
 *   - 使用位掩码管理多个故障状态
 *   - 故障一旦设置，不会自动清除，需要明确的清除条件
 *   - 通过FAULT_MASK_FATAL定义致命故障
 *   - 与其他模块的关系：
 *     - app/task_safety：设置和清除故障
 *     - app/task_control：查询故障状态进行决策
 */

#include "fault_manager.h"

/**
 * @brief   初始化故障管理器
 *
 * @param[out] fm  故障管理器指针
 *
 * 初始化操作：
 *   1. 清零故障锁存掩码
 *
 * @note    初始化后所有故障均为非活跃状态
 */
void fault_manager_init(fault_manager_t *fm)
{
  if(fm == 0)
  {
    return;
  }
  fm->latched_mask = 0U;
}

/**
 * @brief   设置故障
 *
 * @param[out] fm    故障管理器指针
 * @param[in] code   故障代码
 *
 * 操作说明：
 *   - 故障设置为原子操作
 *   - 同一故障可以多次设置，不影响状态
 *   - 故障设置后需要明确的清除条件才能清除
 *
 * @note    故障一旦设置即进入锁存状态，直到满足清除条件
 */
void fault_manager_set(fault_manager_t *fm, fault_code_t code)
{
  if(fm == 0)
  {
    return;
  }
  fm->latched_mask |= (1UL << (uint32_t)code);
}

/**
 * @brief   尝试清除故障
 *
 * @param[out] fm                  故障管理器指针
 * @param[in] code                 故障代码
 * @param[in] clear_condition_met   清除条件是否满足
 *
 * 操作说明：
 *   - 仅当清除条件满足时才会清除故障
 *   - 防止故障状态因瞬时干扰而抖动
 *   - 需要故障消除且系统稳定后才能清除
 *
 * @note    使用条件清除而非直接清除，确保故障真正消除
 */
void fault_manager_try_clear(fault_manager_t *fm, fault_code_t code, bool clear_condition_met)
{
  if((fm == 0) || (clear_condition_met == false))
  {
    return;
  }
  fm->latched_mask &= ~(1UL << (uint32_t)code);
}

/**
 * @brief   检查故障是否锁存
 *
 * @param[in] fm    故障管理器指针
 * @param[in] code  故障代码
 * @return    故障是否锁存
 *
 * 检查流程：
 *   1. 构建故障对应的位掩码
 *   2. 与当前锁存掩码进行按位与运算
 *   3. 返回结果是否为真
 */
bool fault_manager_is_latched(const fault_manager_t *fm, fault_code_t code)
{
  uint32_t mask;
  if(fm == 0)
  {
    return false;
  }
  mask = (1UL << (uint32_t)code);
  return ((fm->latched_mask & mask) != 0U);
}
