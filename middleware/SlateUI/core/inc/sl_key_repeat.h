/**
 * @file    sl_key_repeat.h
 * @brief   SlateUI 按键重复生成器
 *
 * 本模块实现按键长按后的自动重复事件生成，类似 PC 键盘的按键重复机制：
 *   · 按键按下后，经过初始延迟 (SL_KEY_REPEAT_DELAY_MS) 开始生成重复事件。
 *   · 重复间隔从 SL_KEY_REPEAT_INTERVAL_MS 起，随重复次数逐步缩短，
 *     最短不低于 SL_KEY_REPEAT_MIN_INTERVAL_MS，实现加速滚动效果。
 *   · 按键释放时立即停止重复。
 *   · 生成的重复事件 source 字段为 SL_EVT_SOURCE_REPEAT，
 *     可与原始硬件事件区分，避免自反馈循环。
 *
 * 典型用法：
 *   sl_key_repeat_t kr;
 *   sl_key_repeat_init(&kr);
 *   // 在事件分发中：
 *   sl_key_repeat_on_event(&kr, &event);
 *   // 在定时器节拍中：
 *   sl_key_repeat_tick(&kr, delta_ms);
 */

#ifndef SL_KEY_REPEAT_H
#define SL_KEY_REPEAT_H

#include "sl_event.h"
#include <stdint.h>

/* ======================== 按键重复参数配置 ======================== */

/**
 * @brief  首次重复延迟（毫秒）
 * @note   按键按下后，经过此时间才开始生成重复事件；
 *         可在编译选项中覆盖
 */
#ifndef SL_KEY_REPEAT_DELAY_MS
#define SL_KEY_REPEAT_DELAY_MS 500
#endif

/**
 * @brief  初始重复间隔（毫秒）
 * @note   首次重复后的发送间隔；
 *         可在编译选项中覆盖
 */
#ifndef SL_KEY_REPEAT_INTERVAL_MS
#define SL_KEY_REPEAT_INTERVAL_MS 80
#endif

/**
 * @brief  最短重复间隔（毫秒）
 * @note   加速后间隔不会低于此值；
 *         可在编译选项中覆盖
 */
#ifndef SL_KEY_REPEAT_MIN_INTERVAL_MS
#define SL_KEY_REPEAT_MIN_INTERVAL_MS 20
#endif

/**
 * @brief  每次重复间隔缩减步长（毫秒）
 * @note   每生成一个重复事件，间隔缩短此值，直至达到最短间隔；
 *         可在编译选项中覆盖
 */
#ifndef SL_KEY_REPEAT_ACCEL_STEP_MS
#define SL_KEY_REPEAT_ACCEL_STEP_MS 5
#endif

/* ======================== 按键重复状态结构体 ======================== */

/**
 * @brief  按键重复状态结构体
 *
 * 跟踪当前按下的按键、已用时间和重复计数，
 * 由 sl_key_repeat_on_event / sl_key_repeat_tick 驱动状态转移。
 */
typedef struct {
    sl_EventType key;              /**< 当前按下的按键类型（SL_EVT_NONE 表示空闲） */
    int32_t      param;            /**< 按键参数（与原始事件的 param 一致） */
    uint8_t      active;           /**< 重复是否激活（1=延迟中或正在重复，0=空闲） */
    uint16_t     elapsed_ms;       /**< 当前间隔内已用时间（毫秒） */
    uint16_t     current_interval; /**< 当前重复间隔（毫秒），随加速逐步缩短 */
    uint8_t      repeat_count;     /**< 已生成的重复事件计数，用于加速计算 */
} sl_key_repeat_t;

/* ======================== 按键重复接口 ======================== */

/**
 * @brief  初始化按键重复状态
 * @param  kr  指向按键重复状态结构体
 * @note   将状态重置为空闲（key=SL_EVT_NONE, active=0）
 */
void sl_key_repeat_init(sl_key_repeat_t *kr);

/**
 * @brief  处理原始输入事件，更新按键重复状态
 * @param  kr     指向按键重复状态结构体
 * @param  event  指向原始输入事件
 * @note   - 检测到方向键/确认键按下时，启动重复计时
 *         - 检测到按键释放事件时，停止重复
 *         - 忽略 source=SL_EVT_SOURCE_REPEAT 的重复事件，防止自反馈循环
 */
void sl_key_repeat_on_event(sl_key_repeat_t *kr, const sl_Event *event);

/**
 * @brief  按键重复时钟节拍
 * @param  kr        指向按键重复状态结构体
 * @param  delta_ms  距上次调用经过的毫秒数
 * @note   当 active=1 且 elapsed_ms 达到当前间隔时，
 *         自动生成一个重复事件并投递到事件队列；
 *         同时缩短下次间隔（加速），直至达到最短间隔
 */
void sl_key_repeat_tick(sl_key_repeat_t *kr, uint16_t delta_ms);

#endif
