/**
 * @file    task_dmx.h
 * @brief   DMX512 接收任务 — 应用层接口定义
 *
 * ## 职责
 *   dmx_task 是 FreeRTOS 中的一个长期运行任务，负责：
 *   1. 从 HAL 层轮询 DMX 字节事件（poll_byte）
 *   2. 将事件推送到 easyDMX 协议栈（edmx_rx_push_event）
 *   3. 定期设置心跳标志（EDMX_ONLINE_BIT）通知其他任务
 *
 * ## 数据流（完整链路）
 *   [DMX 控制器]
 *        ↓  (DMX512 250kbps 串行)
 *   [USART1 硬件接收]
 *        ↓
 *   [bsp_uart_dmx_irq_handler]  (中断处理：识别 Break + 写入 BSP-FIFO)
 *        ↓
 *   [bsp_uart_dmx_poll_event]  (BSP-FIFO 出队)
 *        ↓
 *   [dmx_task: poll_byte]      (HAL 接口层轮询)
 *        ↓
 *   [edmx_rx_push_event]       (写入 easyDMX-FIFO)
 *        ↓
 *   [edmx_rx_process]          (在 task_control 中调用，状态机解析)
 *        ↓
 *   [edmx_rx_copy_latest]      (task_control 读取最新帧)
 *        ↓
 *   [dmx_strategy_build_intent] (领域层：帧 → 执行意向)
 *        ↓
 *   [task_control: 执行动作]
 *
 * ## 与其他任务的关系
 *   - task_control：调用 dmx_strategy_build_intent() 解读 DMX 帧
 *   - task_safety：通过 EDMX_ONLINE_BIT 事件标志判断 DMX 是否离线
 *
 * ## 初始化顺序
 *   1. bsp_uart_dmx_init()  — 初始化 USART 和 BSP-FIFO
 *   2. edmx_rx_init()       — 初始化 easyDMX 实例（通常在 app 层）
 *   3. app_task_dmx_init()  — 绑定配置（app/rx/event_group）
 *   4. xTaskCreate(dmx_task) — 创建 FreeRTOS 任务
 */

#ifndef APP_TASK_DMX_H
#define APP_TASK_DMX_H

#include "../../project/inc/freertos_app.h"
#include "../app_core.h"
#include "../../middleware/easyDMX/easy_dmx.h"

/**
 * @brief   DMX 任务配置结构
 *
 * 在创建 dmx_task 之前，由系统初始化代码填充本结构，
 * 并调用 app_task_dmx_init() 完成绑定。
 */
typedef struct
{
  /** @brief 应用核心实例（含 HAL 接口和参数）*/
  app_core_t *app;
  /** @brief easyDMX 接收器实例（由 app 层分配） */
  edmx_rx_t *rx;
  /** @brief FreeRTOS 事件组（用于通知其他任务 DMX 在线状态）*/
  EventGroupHandle_t event_group;
  /** @brief 心跳标志位（通知 DMX 在线），如 EDMX_ONLINE_BIT */
  EventBits_t hb_bit;
} app_task_dmx_cfg_t;

/**
 * @brief   初始化 DMX 任务
 *
 * @param[in] cfg  任务配置（传入后内部保存一份副本）
 *
 * @note    cfg 传入后内部会拷贝一份，所以传入后 cfg 可安全释放
 */
void app_task_dmx_init(const app_task_dmx_cfg_t *cfg);

/**
 * @brief   DMX512 接收任务主体（FreeRTOS 任务函数）
 *
 * 工作流程：
 *   1. 等待 app 和 rx 有效（防御性检查）
 *   2. 轮询 HAL.poll_byte()，将每个字节事件推入 easyDMX
 *   3. 每轮循环设置一次心跳标志
 *   4. 延时 1ms 后重复
 *
 * @param[in] pvParameters 标准 FreeRTOS 参数（本任务不使用）
 *
 * @note    轮询间隔 1ms，在 250kbps 下一帧最多 513 字节 ≈ 23ms，
 *          期间该任务会分多次处理，有足够实时性
 */
void dmx_task(void *pvParameters);

#endif
