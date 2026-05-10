/**
 * @file    app_task_common.c
 * @brief   任务公共功能实现
 *
 * 任务公共模块，提供：
 *   - 队列操作函数（发送最新命令，必要时丢弃旧命令）
 *   - 事件标志操作函数（设置状态和故障标志）
 *   - 安全操作函数（发送安全关闭命令）
 *
 * 设计思路：
 *   - 提供通用的任务间通信机制
 *   - 确保命令发送的可靠性和优先级
 *   - 统一管理事件标志的设置和读取
 *   - 与其他模块的关系：
 *     - app/task_*：使用这些公共函数进行任务间通信
 *     - freertos_app.c：创建队列和事件组供这些函数使用
 */

#include "app_task_common.h"

/**
 * @brief   发送最新命令到队列，必要时丢弃旧命令
 *
 * @param[in] q         队列句柄
 * @param[in] cmd       命令指针
 * @param[in] to_front  是否发送到队列前端（pdTRUE=高优先级）
 *
 * 操作流程：
 *   1. 尝试发送命令到队列
 *   2. 如果队列已满，先接收并丢弃一个旧命令
 *   3. 再次尝试发送新命令
 *
 * @note    此函数确保最新的命令能够被处理，旧命令会被丢弃
 */
void app_task_queue_send_latest(QueueHandle_t q, const actuator_cmd_t *cmd, BaseType_t to_front)
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

/**
 * @brief   设置状态事件标志
 *
 * @param[in] eg  事件组句柄
 * @param[in] st  机器状态
 *
 * 操作流程：
 *   1. 清除所有状态标志
 *   2. 根据机器状态设置对应的状态标志
 *
 * @note    此函数确保同一时间只有一个状态标志被设置
 */
void app_task_set_state_bits(EventGroupHandle_t eg, machine_state_t st)
{
  EventBits_t clear_mask;
  EventBits_t set_mask;

  if(eg == 0)
  {
    return;
  }

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

  (void)xEventGroupClearBits(eg, clear_mask);
  if(set_mask != 0U)
  {
    (void)xEventGroupSetBits(eg, set_mask);
  }
}

/**
 * @brief   设置故障事件标志
 *
 * @param[in] eg    事件组句柄
 * @param[in] mask  故障掩码
 *
 * 操作流程：
 *   1. 清除所有故障标志
 *   2. 根据故障掩码设置对应的故障标志
 *
 * @note    此函数可以同时设置多个故障标志
 */
void app_task_set_fault_bits(EventGroupHandle_t eg, uint32_t mask)
{
  EventBits_t clear_mask;
  EventBits_t set_mask;

  if(eg == 0)
  {
    return;
  }

  clear_mask = EVT_FAULT_E1_BIT | EVT_FAULT_E2_BIT | EVT_FAULT_E3_BIT | EVT_FAULT_E4_BIT | EVT_FAULT_E5_BIT;
  set_mask = 0U;
  if((mask & FAULT_MASK_E1) != 0U) { set_mask |= EVT_FAULT_E1_BIT; }
  if((mask & FAULT_MASK_E2) != 0U) { set_mask |= EVT_FAULT_E2_BIT; }
  if((mask & FAULT_MASK_E3) != 0U) { set_mask |= EVT_FAULT_E3_BIT; }
  if((mask & FAULT_MASK_E4) != 0U) { set_mask |= EVT_FAULT_E4_BIT; }
  if((mask & FAULT_MASK_E5) != 0U) { set_mask |= EVT_FAULT_E5_BIT; }

  (void)xEventGroupClearBits(eg, clear_mask);
  if(set_mask != 0U)
  {
    (void)xEventGroupSetBits(eg, set_mask);
  }
}

/**
 * @brief   从事件标志读取故障掩码
 *
 * @param[in] bits  事件标志
 * @return    故障掩码
 *
 * 操作流程：
 *   根据事件标志中的故障位，生成对应的故障掩码
 *
 * @note    此函数是app_task_set_fault_bits的反向操作
 */
uint32_t app_task_read_fault_mask_from_events(EventBits_t bits)
{
  /* Transitional helper kept for compatibility with old event-bit mirror logic. */
  uint32_t mask;

  mask = 0U;
  if((bits & EVT_FAULT_E1_BIT) != 0U) { mask |= FAULT_MASK_E1; }
  if((bits & EVT_FAULT_E2_BIT) != 0U) { mask |= FAULT_MASK_E2; }
  if((bits & EVT_FAULT_E3_BIT) != 0U) { mask |= FAULT_MASK_E3; }
  if((bits & EVT_FAULT_E4_BIT) != 0U) { mask |= FAULT_MASK_E4; }
  if((bits & EVT_FAULT_E5_BIT) != 0U) { mask |= FAULT_MASK_E5; }
  return mask;
}

/**
 * @brief   发送安全关闭命令（高优先级）
 *
 * @param[in] q  命令队列句柄
 *
 * 操作流程：
 *   1. 创建安全关闭命令
 *   2. 设置高优先级
 *   3. 发送到队列前端
 *
 * @note    此函数用于紧急情况下的安全关闭操作
 */
void app_task_send_safe_off_high_prio(QueueHandle_t q)
{
  actuator_cmd_t cmd;

  cmd.type = ACT_CMD_SAFE_OFF;
  cmd.priority = 10U;
  cmd.user_mode = false;
  cmd.igniter_delay_sec = 0U;
  cmd.oil_lock_delay_sec = 0U;
  cmd.fire_duration_ms = 0U;
  app_task_queue_send_latest(q, &cmd, pdTRUE);
}
