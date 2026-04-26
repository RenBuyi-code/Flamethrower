/**
 * @file    sl_tween.c
 * @brief   SlateUI 整数插值动画引擎实现
 *
 * 本文件实现轻量级整数插值动画的核心算法：
 *   · 使用定点数运算（Q8 格式，256 = 1.0），无浮点依赖。
 *   · 四种缓动曲线：Linear / Ease-In / Ease-Out / Ease-In-Out。
 *   · Ease-In/Out 使用二次函数近似，运算量极小。
 *   · 所有中间值使用 int64_t 防止溢出。
 *
 * 定点数约定：
 *   progress_256 = (elapsed_ms * 256) / duration_ms
 *   范围 [0, 256]，其中 0 = 起始，256 = 结束
 */

#include "../inc/sl_tween.h"
#include <string.h>

/* ======================== 缓动曲线内部函数 ======================== */

/**
 * @brief  缓入曲线（二次加速）
 * @param  t  归一化进度 [0, 256]
 * @retval 缓动后的进度值 [0, 256]
 * @note   公式：t² / 256（Q8 定点）
 */
static int32_t ease_in(int32_t t) {
    int64_t tt = (int64_t)t * t;
    return (int32_t)(tt / 256);
}

/**
 * @brief  缓出曲线（二次减速）
 * @param  t  归一化进度 [0, 256]
 * @retval 缓动后的进度值 [0, 256]
 * @note   公式：1 - (1-t)² （Q8 定点）
 */
static int32_t ease_out(int32_t t) {
    int32_t inv = 256 - t;
    int64_t ii = (int64_t)inv * inv;
    return 256 - (int32_t)(ii / 256);
}

/**
 * @brief  缓入缓出曲线（先加速后减速）
 * @param  t  归一化进度 [0, 256]
 * @retval 缓动后的进度值 [0, 256]
 * @note   前半段缓入，后半段缓出（Q8 定点）
 */
static int32_t ease_in_out(int32_t t) {
    if (t < 128) {
        int32_t t2 = t * 2;
        int64_t tt = (int64_t)t2 * t2;
        return (int32_t)(tt / 512);
    } else {
        int32_t inv = (256 - t) * 2;
        int64_t ii = (int64_t)inv * inv;
        return 256 - (int32_t)(ii / 512);
    }
}

/**
 * @brief  根据曲线类型应用缓动函数
 * @param  curve  缓动曲线类型
 * @param  t      归一化进度 [0, 256]
 * @retval 缓动后的进度值 [0, 256]
 */
static int32_t apply_curve(sl_tween_curve_t curve, int32_t t) {
    switch (curve) {
    case SL_TWEEN_EASE_IN:     return ease_in(t);
    case SL_TWEEN_EASE_OUT:    return ease_out(t);
    case SL_TWEEN_EASE_IN_OUT: return ease_in_out(t);
    default:                   return t;
    }
}

/* ======================== 插值动画接口实现 ======================== */

/**
 * @brief  启动一个插值动画
 * @param  t            指向插值动画状态结构体
 * @param  start        起始值
 * @param  end          目标值
 * @param  duration_ms  动画总时长（毫秒）
 * @param  curve        缓动曲线类型
 * @note   初始化所有字段，active=true, finished=false, current=start
 */
void sl_tween_start(sl_tween_t *t, int32_t start, int32_t end,
                    uint16_t duration_ms, sl_tween_curve_t curve) {
    if (!t) return;
    t->start = start;
    t->end = end;
    t->current = start;
    t->duration_ms = duration_ms;
    t->elapsed_ms = 0;
    t->curve = curve;
    t->active = true;
    t->finished = false;
}

/**
 * @brief  推进插值动画时间
 * @param  t         指向插值动画状态结构体
 * @param  delta_ms  距上次调用经过的毫秒数
 *
 * 计算流程：
 *   1. 若 active=false，直接返回；
 *   2. 累加 elapsed_ms；
 *   3. 若 elapsed_ms >= duration_ms，动画结束：current=end, active=false, finished=true；
 *   4. 否则计算归一化进度 progress_256，应用缓动曲线，
 *      线性插值：current = start + (end - start) * curved / 256
 */
void sl_tween_tick(sl_tween_t *t, uint16_t delta_ms) {
    if (!t || !t->active) return;

    t->elapsed_ms += delta_ms;

    if (t->elapsed_ms >= t->duration_ms) {
        t->current = t->end;
        t->active = false;
        t->finished = true;
        return;
    }

    int32_t progress_256;
    if (t->duration_ms > 0) {
        progress_256 = ((int32_t)t->elapsed_ms * 256) / t->duration_ms;
    } else {
        progress_256 = 256;
    }

    int32_t curved = apply_curve(t->curve, progress_256);
    t->current = t->start + ((t->end - t->start) * curved) / 256;
}

/**
 * @brief  获取当前插值结果
 * @param  t  指向插值动画状态结构体（只读）
 * @retval 当前插值计算结果；t 为 NULL 时返回 0
 */
int32_t sl_tween_get_value(const sl_tween_t *t) {
    return t ? t->current : 0;
}

/**
 * @brief  查询动画是否正在运行
 * @param  t  指向插值动画状态结构体（只读）
 * @retval true   动画进行中
 * @retval false  动画已停止或 t 为 NULL
 */
bool sl_tween_is_active(const sl_tween_t *t) {
    return t ? t->active : false;
}

/**
 * @brief  查询动画是否已完成
 * @param  t  指向插值动画状态结构体（只读）
 * @retval true   动画已完成
 * @retval false  动画未完成或 t 为 NULL
 */
bool sl_tween_is_finished(const sl_tween_t *t) {
    return t ? t->finished : false;
}
