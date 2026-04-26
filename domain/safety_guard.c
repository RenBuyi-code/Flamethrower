/**
 * @file    safety_guard.c
 * @brief   安全防护评估实现
 *
 * 安全防护模块，负责：
 *   - 评估系统安全状态
 *   - 根据输入条件返回安全动作
 *   - 防止危险操作执行
 *
 * 安全评估逻辑（优先级从高到低）：
 *   1. 致命故障 → 强制停止
 *   2. 非用户模式 → 锁定
 *   3. 泄压请求 → 强制泄压
 *   4. 倾斜故障 → 强制停止
 *   5. 电压异常 → 强制停止
 *   6. 以上都不是 → 允许点火
 *
 * 设计思路：
 *   - 使用单一函数进行安全评估
 *   - 按优先级逐项检查条件
 *   - 返回最严重的安全动作
 *   - 与其他模块的关系：
 *     - app/task_control：使用评估结果决定执行动作
 *     - domain/fault_manager：查询故障状态
 */

#include "safety_guard.h"
#include "fault_manager.h"

/**
 * @brief   评估安全状态
 *
 * @param[in] in  安全评估输入参数指针
 * @return    安全动作
 *
 * 安全动作定义：
 *   - SAFETY_FORCE_STOP：强制停止所有执行器
 *   - SAFETY_LOCKED：锁定状态，停止执行器但保持监控
 *   - SAFETY_FORCE_RELIEF：强制泄压
 *   - SAFETY_ALLOW_FIRE：允许点火
 *
 * 评估优先级：
 *   1. 致命故障检查
 *   2. 用户模式检查
 *   3. 泄压请求检查
 *   4. 倾斜故障检查
 *   5. 电压异常检查
 *   6. 默认允许点火
 *
 * @note    此函数是安全系统的核心，任何时候都应调用
 */
safety_action_t safety_guard_eval(const safety_eval_input_t *in)
{
  if(in == 0)
  {
    return SAFETY_FORCE_STOP;
  }

  /** 优先级1：检查致命故障 */
  if((in->latched_fault_mask & FAULT_MASK_FATAL) != 0U)
  {
    return SAFETY_FORCE_STOP;
  }

  /** 优先级2：检查用户模式 */
  if(in->in_user_mode == 0)
  {
    return SAFETY_LOCKED;
  }

  /** 优先级3：检查泄压请求 */
  if(in->relief_requested != 0)
  {
    return SAFETY_FORCE_RELIEF;
  }

  /** 优先级4：检查倾斜故障 */
  if(in->tilt_fault_active != 0)
  {
    return SAFETY_FORCE_STOP;
  }

  /** 优先级5：检查电压状态 */
  if(in->voltage_ok == 0)
  {
    return SAFETY_FORCE_STOP;
  }

  /** 默认：允许点火 */
  return SAFETY_ALLOW_FIRE;
}
