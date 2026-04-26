/**
 * @file    app_task_common.h
 * @brief   任务公共功能头文件
 *
 * 任务公共模块的头文件，定义了：
 *   - 队列操作函数声明
 *   - 事件标志操作函数声明
 *   - 安全操作函数声明
 *
 * 设计思路：
 *   - 提供通用的任务间通信接口
 *   - 确保命令发送的可靠性和优先级
 *   - 统一管理事件标志的设置和读取
 *   - 与其他模块的关系：
 *     - app/task_*：使用这些公共函数进行任务间通信
 *     - freertos_app.c：创建队列和事件组供这些函数使用
 */

#ifndef APP_TASK_COMMON_H
#define APP_TASK_COMMON_H

#include "app_task_shared.h"
#include "../domain/fault_manager.h"

/**
 * @brief   发送最新命令到队列，必要时丢弃旧命令
 *
 * @param[in] q         队列句柄
 * @param[in] cmd       命令指针
 * @param[in] to_front  是否发送到队列前端（pdTRUE=高优先级）
 *
 * 确保最新的命令能够被处理，旧命令会被丢弃
 */
void app_task_queue_send_latest(QueueHandle_t q, const actuator_cmd_t *cmd, BaseType_t to_front);

/**
 * @brief   设置状态事件标志
 *
 * @param[in] eg  事件组句柄
 * @param[in] st  机器状态
 *
 * 清除所有状态标志，根据机器状态设置对应的状态标志
 */
void app_task_set_state_bits(EventGroupHandle_t eg, machine_state_t st);

/**
 * @brief   设置故障事件标志
 *
 * @param[in] eg    事件组句柄
 * @param[in] mask  故障掩码
 *
 * 清除所有故障标志，根据故障掩码设置对应的故障标志
 */
void app_task_set_fault_bits(EventGroupHandle_t eg, uint32_t mask);

/**
 * @brief   从事件标志读取故障掩码
 *
 * @param[in] bits  事件标志
 * @return    故障掩码
 *
 * 根据事件标志中的故障位，生成对应的故障掩码
 */
uint32_t app_task_read_fault_mask_from_events(EventBits_t bits);

/**
 * @brief   发送安全关闭命令（高优先级）
 *
 * @param[in] q  命令队列句柄
 *
 * 发送安全关闭命令到队列前端，确保紧急情况下的安全操作
 */
void app_task_send_safe_off_high_prio(QueueHandle_t q);

#endif
