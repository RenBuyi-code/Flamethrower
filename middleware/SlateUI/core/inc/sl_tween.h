/**
 * @file    sl_tween.h
 * @brief   SlateUI 整数插值动画引擎
 *
 * 本模块实现轻量级的整数插值动画，用于光标平滑移动、页面过渡等场景：
 *   · 支持 Linear / Ease-In / Ease-Out / Ease-In-Out 四种缓动曲线。
 *   · 所有运算使用整数，无浮点依赖，适合无 FPU 的 MCU。
 *   · 由 sl_tween_tick() 驱动时间推进，与应用主循环节拍同步。
 *   · 动画完成后 finished 标志置位，由调用方检查并处理。
 *
 * 典型用法：
 *   sl_tween_t tw;
 *   sl_tween_start(&tw, 0, 100, 200, SL_TWEEN_EASE_OUT);
 *   // 主循环中：
 *   sl_tween_tick(&tw, delta_ms);
 *   int32_t val = sl_tween_get_value(&tw);
 *   if (sl_tween_is_finished(&tw)) { ... }
 */

#ifndef SL_TWEEN_H
#define SL_TWEEN_H

#include <stdint.h>
#include <stdbool.h>

/* ======================== 缓动曲线类型定义 ======================== */

/**
 * @brief  缓动曲线类型枚举
 *
 * 定义插值动画的速率变化曲线：
 *   - LINEAR:      匀速运动
 *   - EASE_IN:     慢启动，快结束（加速）
 *   - EASE_OUT:    快启动，慢结束（减速），最常用于 UI 动画
 *   - EASE_IN_OUT: 慢启动慢结束，中间加速
 */
typedef enum {
    SL_TWEEN_LINEAR    = 0,  /**< 线性匀速 */
    SL_TWEEN_EASE_IN,        /**< 缓入（加速） */
    SL_TWEEN_EASE_OUT,       /**< 缓出（减速） */
    SL_TWEEN_EASE_IN_OUT     /**< 缓入缓出 */
} sl_tween_curve_t;

/* ======================== 插值动画状态结构体 ======================== */

/**
 * @brief  插值动画状态结构体
 *
 * 记录动画的起止值、当前进度和缓动曲线，
 * 由 sl_tween_start() 初始化，sl_tween_tick() 推进。
 */
typedef struct {
    int32_t           start;        /**< 起始值 */
    int32_t           end;          /**< 目标值 */
    int32_t           current;      /**< 当前插值结果 */
    uint16_t          duration_ms;  /**< 动画总时长（毫秒） */
    uint16_t          elapsed_ms;   /**< 已用时间（毫秒） */
    sl_tween_curve_t  curve;        /**< 缓动曲线类型 */
    bool              active;       /**< 动画是否进行中（true=运行，false=停止） */
    bool              finished;     /**< 动画是否已完成（true=完成，单次触发） */
} sl_tween_t;

/* ======================== 插值动画接口 ======================== */

/**
 * @brief  启动一个插值动画
 * @param  t            指向插值动画状态结构体
 * @param  start        起始值
 * @param  end          目标值
 * @param  duration_ms  动画总时长（毫秒），0 则立即跳到目标值
 * @param  curve        缓动曲线类型
 * @note   调用后 active=true, finished=false, current=start；
 *         若 duration_ms=0，则 current 立即设为 end，finished=true
 */
void    sl_tween_start(sl_tween_t *t, int32_t start, int32_t end,
                       uint16_t duration_ms, sl_tween_curve_t curve);

/**
 * @brief  推进插值动画时间
 * @param  t         指向插值动画状态结构体
 * @param  delta_ms  距上次调用经过的毫秒数
 * @note   - 若 active=false，此函数不做任何操作
 *         - 累加 elapsed_ms，根据进度和缓动曲线计算 current
 *         - 当 elapsed_ms >= duration_ms 时，current=end，
 *           active=false, finished=true
 */
void    sl_tween_tick(sl_tween_t *t, uint16_t delta_ms);

/**
 * @brief  获取当前插值结果
 * @param  t  指向插值动画状态结构体（只读）
 * @retval 当前插值计算结果（start ~ end 之间的值）
 */
int32_t sl_tween_get_value(const sl_tween_t *t);

/**
 * @brief  查询动画是否正在运行
 * @param  t  指向插值动画状态结构体（只读）
 * @retval true   动画进行中
 * @retval false  动画已停止或未启动
 */
bool    sl_tween_is_active(const sl_tween_t *t);

/**
 * @brief  查询动画是否已完成
 * @param  t  指向插值动画状态结构体（只读）
 * @retval true   动画已完成（单次触发，需重新 start 才能再次运行）
 * @retval false  动画未完成或未启动
 */
bool    sl_tween_is_finished(const sl_tween_t *t);

#endif
