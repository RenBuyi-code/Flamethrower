/**
 * @file    app_fsm.h
 * @brief   应用核心头文件
 *
 * 应用核心模块的头文件，定义了：
 *   - 应用核心结构体（app_fsm_t）
 *   - 核心功能函数声明
 *
 * 设计思路：
 *   - 集中管理系统的共享状态和资源
 *   - 提供统一的接口给各个任务模块
 *   - 与SlateUI解耦，可独立使用
 *
 * 核心组件：
 *   - HAL绑定：硬件抽象层接口
 *   - 机器状态：系统状态管理
 *   - 故障管理：故障检测和处理
 *   - 事件日志：系统事件记录
 *   - 系统参数：用户配置和运行参数
 */

#ifndef APP_APP_FSM_H
#define APP_APP_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "../bsp/at32f415/bsp_at32f415.h"
#include "../cfg/system_config.h"
#include "rules/event_log.h"
#include "rules/fault_manager.h"
#include "rules/state_machine.h"

/**
 * @brief   应用核心结构体
 *
 * 应用核心是整个系统的中央管理结构，包含：
 *   - 硬件抽象层绑定
 *   - 机器状态管理
 *   - 故障管理
 *   - 事件日志
 *   - 系统参数
 *
 * 所有任务可以通过此结构体共享一个一致的运行时上下文。
 */
typedef struct
{
  /** @brief 硬件抽象层绑定，提供底层硬件访问接口 */
  bsp_hal_bundle_t hal;
  /** @brief 机器状态管理，处理系统状态转换 */
  state_machine_t machine;
  /** @brief 故障管理，检测和处理系统故障 */
  fault_manager_t faults;
  /** @brief 事件日志，记录系统事件 */
  event_log_t events;
  /** @brief 系统参数，包含用户配置和运行参数 */
  system_params_t params;
} app_fsm_t;

/**
 * @brief   初始化应用核心
 *
 * @param[in] core  应用核心实例指针
 *
 * 初始化系统各组件，包括HAL绑定、状态机、故障管理、事件日志和默认参数。
 */
void app_fsm_init(app_fsm_t *core);

/**
 * @brief   加载参数或使用默认值
 *
 * @param[in] core  应用核心实例指针
 *
 * 尝试从存储中加载参数，失败则使用默认值，并对参数进行校验和修正。
 */
void app_fsm_load_or_default_params(app_fsm_t *core);

bool app_fsm_apply_params(app_fsm_t *core, const system_params_t *params);
bool app_fsm_get_params_snapshot(const app_fsm_t *core, system_params_t *out);

/**
 * @brief   记录系统事件
 *
 * @param[in] core     应用核心实例指针
 * @param[in] code     事件代码
 * @param[in] ts_ms    事件时间戳（毫秒）
 *
 * 将系统事件推入事件日志队列。
 */
void app_fsm_log(app_fsm_t *core, uint16_t code, uint32_t ts_ms);

/**
 * @brief   切换机器状态
 *
 * @param[in] core       应用核心实例指针
 * @param[in] next       目标状态
 * @param[in] event_code 触发事件代码
 * @param[in] ts_ms      事件时间戳（毫秒）
 * @return    状态转换是否成功
 *
 * 执行状态转换，记录状态转换事件，并输出日志。
 */
bool app_fsm_transition(app_fsm_t *core, machine_state_t next, uint16_t event_code, uint32_t ts_ms);

#endif
