/**
 * @file    event_log.c
 * @brief   事件日志实现
 *
 * 事件日志模块，负责：
 *   - 管理系统事件日志记录
 *   - 提供循环缓冲区存储事件
 *   - 支持事件查询和遍历
 *
 * 设计思路：
 *   - 使用循环缓冲区记录最新事件
 *   - 记录事件代码和时间戳
 *   - 与其他模块的关系：
 *     - app_core：使用事件日志记录系统状态变化
 */

#include "event_log.h"

/**
 * @brief   初始化事件日志
 *
 * @param[out] log  事件日志结构体指针
 *
 * 初始化操作：
 *   1. 重置日志头指针
 *   2. 清空所有日志数据
 */
void event_log_init(event_log_t *log)
{
  uint16_t i;
  if(log == 0)
  {
    return;
  }
  log->head = 0U;
  for(i = 0U; i < EVENT_LOG_CAPACITY; ++i)
  {
    log->data[i].code = 0U;
    log->data[i].timestamp_ms = 0U;
  }
}

/**
 * @brief   推送事件到日志
 *
 * @param[out] log     事件日志结构体指针
 * @param[in] code     事件代码
 * @param[in] ts_ms    事件时间戳（毫秒）
 *
 * 操作流程：
 *   1. 计算日志缓冲区当前索引
 *   2. 写入事件数据（代码和时间戳）
 *   3. 更新日志头指针
 *
 * @note    使用循环缓冲区，新事件会覆盖旧事件
 */
void event_log_push(event_log_t *log, uint16_t code, uint32_t ts_ms)
{
  uint16_t index;
  if(log == 0)
  {
    return;
  }
  index = (uint16_t)(log->head % EVENT_LOG_CAPACITY);
  log->data[index].code = code;
  log->data[index].timestamp_ms = ts_ms;
  log->head = (uint16_t)(log->head + 1U);
}
