/**
 * @file    task_dmx.c
 * @brief   DMX512 接收任务 — FreeRTOS 任务实现
 *
 * ## 任务职责
 *   dmx_task 是一个数据搬运任务，它本身不进行 DMX 协议解析，
 *   只是将 HAL 层（bsp_uart）的事件源源不断地喂入 easyDMX 协议栈。
 *
 *   解析工作由 task_control 在调用 edmx_rx_process() 时完成。
 *
 * ## 为什么这样设计（任务分工）
 *   - dmx_task：高优先级，轮询硬件，数据搬运（耗时少但要求快）
 *   - task_control：中等优先级，协议解析 + 执行控制（业务逻辑）
 *
 * ## 性能考虑
 *   - 每次循环最多处理 BSP-FIFO 中的全部事件（最大 256 字节）
 *   - 延时 1ms 防止任务独占 CPU，同时保证 250kbps 下不丢失数据
 *     （1ms ≈ 250 字节，256 字节 FIFO 有余量）
 *
 * ## 事件标志
 *   每轮循环会设置一次 hb_bit（DMX 心跳），
 *   用于通知其他任务："DMX 控制器仍在发送数据"。
 */

#include "task_dmx.h"

/** @brief DMX 任务全局配置（静态单例） */
static app_task_dmx_cfg_t s_task_dmx_cfg;

/**
 * @brief   初始化 DMX 任务配置
 *
 * @param[in] cfg  配置结构体指针（传入后内部拷贝一份）
 *
 * 当 cfg == NULL 时，清除配置（用于异常恢复）
 */
void app_task_dmx_init(const app_task_dmx_cfg_t *cfg)
{
  if(cfg == 0)
  {
    s_task_dmx_cfg.app = 0;
    s_task_dmx_cfg.rx = 0;
    s_task_dmx_cfg.event_group = 0;
    s_task_dmx_cfg.hb_bit = 0U;
    return;
  }

  s_task_dmx_cfg = *cfg;
}

/**
 * @brief   DMX512 接收任务主体
 *
 * @param[in] pvParameters 未使用（标准 FreeRTOS 接口）
 *
 * ## 主循环逻辑
 *   for (;;) {
 *       if (app/rx 未初始化) { 延迟 10ms 后重试; continue; }
 *
 *       // 消费 BSP-FIFO 中全部事件（通常每次 1~几十字节）
 *       while (hal.poll_byte(&byte, &is_break)) {
 *           edmx_rx_push_event(rx, {byte, is_break});
 *       }
 *
 *       // 通知其他任务："DMX 在线"
 *       xEventGroupSetBits(event_group, hb_bit);
 *
 *       vTaskDelay(pdMS_TO_TICKS(1));
 *   }
 *
 * ## 关键设计点
 *   - "消费全部"而非"每轮只取一个"：减少循环次数
 *   - 心跳每轮都设置：允许 task_safety 等任务监控 DMX 在线状态
 *   - 1ms 延时：平衡实时性与 CPU 占用
 */
void dmx_task(void *pvParameters)
{
  edmx_event_t evt;
  bool is_break;
  uint8_t b;
  (void)pvParameters;

  for(;;)
  {
    /* 防御性检查：等待 app 和 rx 初始化完成 */
    if((s_task_dmx_cfg.app == 0) || (s_task_dmx_cfg.rx == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    /* 从 BSP 层轮询所有待处理事件，推入 easyDMX */
    while(s_task_dmx_cfg.app->hal.dmx.poll_byte(s_task_dmx_cfg.app->hal.dmx.ctx, &b, &is_break))
    {
      evt.byte = b;
      evt.flags = is_break ? EDMX_EVENT_FLAG_BREAK : 0U;
      (void)edmx_rx_push_event(s_task_dmx_cfg.rx, &evt);
    }

    /* 设置心跳标志，通知其他任务 DMX 信号在线 */
    if(s_task_dmx_cfg.event_group != 0)
    {
      (void)xEventGroupSetBits(s_task_dmx_cfg.event_group, s_task_dmx_cfg.hb_bit);
    }

    /* 延时 1ms，防止 CPU 忙等待，同时保持足够的轮询频率 */
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
