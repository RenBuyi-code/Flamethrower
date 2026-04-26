/**
 * @file    sl_key_repeat.c
 * @brief   SlateUI 按键重复生成器实现
 *
 * 本文件实现按键长按后的自动重复事件生成：
 *   · 方向键（上/下/左/右）按下时启动重复计时；
 *   · 首次延迟 SL_KEY_REPEAT_DELAY_MS 后开始重复；
 *   · 重复间隔随次数逐步缩短（加速），最短不低于 SL_KEY_REPEAT_MIN_INTERVAL_MS；
 *   · 确认键/返回键按下时立即停止重复；
 *   · 生成的重复事件 source=SL_EVT_SOURCE_REPEAT，防止自反馈循环。
 */

#include "../inc/sl_key_repeat.h"
#include <string.h>

/**
 * @brief  初始化按键重复状态
 * @param  kr  指向按键重复状态结构体
 * @note   将所有字段清零，key 设为 SL_EVT_NONE 表示空闲
 */
void sl_key_repeat_init(sl_key_repeat_t *kr) {
    memset(kr, 0, sizeof(*kr));
    kr->key = SL_EVT_NONE;
}

/**
 * @brief  处理原始输入事件，更新按键重复状态
 * @param  kr     指向按键重复状态结构体
 * @param  event  指向原始输入事件
 *
 * 状态转移逻辑：
 *   - 方向键（UP/DOWN/LEFT/RIGHT）按下：启动重复计时，
 *     初始间隔为 SL_KEY_REPEAT_DELAY_MS（首次延迟）
 *   - 确认键/返回键按下：立即停止重复（active=0）
 *   - source=SL_EVT_SOURCE_REPEAT 的事件：忽略，防止自反馈循环
 */
void sl_key_repeat_on_event(sl_key_repeat_t *kr, const sl_Event *event) {
    if (!kr || !event) return;

    if (event->source == SL_EVT_SOURCE_REPEAT) return;

    if (event->type == SL_EVT_KEY_UP || event->type == SL_EVT_KEY_DOWN ||
        event->type == SL_EVT_KEY_LEFT || event->type == SL_EVT_KEY_RIGHT) {
        kr->key = event->type;
        kr->param = event->param;
        kr->active = 1;
        kr->elapsed_ms = 0;
        kr->current_interval = SL_KEY_REPEAT_DELAY_MS;
        kr->repeat_count = 0;
    } else if (event->type == SL_EVT_KEY_ENTER || event->type == SL_EVT_KEY_BACK) {
        kr->active = 0;
        kr->key = SL_EVT_NONE;
    }
}

/**
 * @brief  按键重复时钟节拍
 * @param  kr        指向按键重复状态结构体
 * @param  delta_ms  距上次调用经过的毫秒数
 *
 * 重复生成逻辑：
 *   1. 若 active=0，直接返回；
 *   2. 累加 elapsed_ms；
 *   3. 当 elapsed_ms >= current_interval 时：
 *      a. 重置 elapsed_ms 为 0；
 *      b. 递增 repeat_count；
 *      c. 生成一个重复事件（source=SL_EVT_SOURCE_REPEAT）并投递到事件队列；
 *      d. 首次重复后切换间隔为 SL_KEY_REPEAT_INTERVAL_MS；
 *      e. 后续每次重复缩短 SL_KEY_REPEAT_ACCEL_STEP_MS，直至最短间隔
 */
void sl_key_repeat_tick(sl_key_repeat_t *kr, uint16_t delta_ms) {
    if (!kr || !kr->active) return;

    kr->elapsed_ms += delta_ms;

    if (kr->elapsed_ms >= kr->current_interval) {
        kr->elapsed_ms = 0;
        kr->repeat_count++;

        sl_Event repeat_evt;
        repeat_evt.type = kr->key;
        repeat_evt.param = kr->param;
        repeat_evt.source = SL_EVT_SOURCE_REPEAT;
        sl_event_post(&repeat_evt);

        if (kr->repeat_count == 1) {
            kr->current_interval = SL_KEY_REPEAT_INTERVAL_MS;
        } else if (kr->current_interval > SL_KEY_REPEAT_MIN_INTERVAL_MS) {
            kr->current_interval -= SL_KEY_REPEAT_ACCEL_STEP_MS;
            if (kr->current_interval < SL_KEY_REPEAT_MIN_INTERVAL_MS) {
                kr->current_interval = SL_KEY_REPEAT_MIN_INTERVAL_MS;
            }
        }
    }
}
