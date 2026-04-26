/**
 * @file    ui_services.c
 * @brief   UI服务实现
 *
 * UI服务模块，提供：
 *   - 机器状态快照管理（供显示页面使用）
 *   - 设置页面的参数保存适配器
 *
 * 设计思路：
 *   - 保持文件小巧简洁
 *   - 提供统一的接口给UI页面
 *   - 作为UI页面和应用核心之间的桥梁
 *   - 与其他模块的关系：
 *     - app_core：获取系统状态
 *     - task_ui：提供服务给UI任务
 *     - UI页面：使用服务获取状态和保存设置
 */

#include "ui_services.h"
#include <string.h>

/** @brief 机器状态快照 */
static ui_machine_snapshot_t s_machine_snapshot;
/** @brief 机器状态快照有效性标志 */
static bool s_machine_snapshot_valid;
/** @brief 设置页面回调函数 */
static ui_setting_handlers_t s_setting_handlers;

/**
 * @brief   设置机器状态快照
 *
 * @param[in] snapshot  机器状态快照指针
 * @return    快照是否发生变化
 *
 * 操作流程：
 *   1. 检查参数有效性
 *   2. 检查快照是否发生变化
 *   3. 更新快照
 *   4. 设置有效性标志
 *   5. 返回变化状态
 */
bool ui_service_set_machine_snapshot(const ui_machine_snapshot_t *snapshot)
{
  bool changed;

  if(snapshot == 0)
  {
    return false;
  }

  changed = (s_machine_snapshot_valid == false) ||
            (memcmp(&s_machine_snapshot, snapshot, sizeof(s_machine_snapshot)) != 0);

  s_machine_snapshot = *snapshot;
  s_machine_snapshot_valid = true;
  return changed;
}

/**
 * @brief   获取机器状态快照
 *
 * @param[out] out  输出参数，用于存储快照
 * @return    快照是否有效
 *
 * 操作流程：
 *   1. 检查参数有效性和快照有效性
 *   2. 复制快照到输出参数
 *   3. 返回快照有效性
 */
bool ui_service_get_machine_snapshot(ui_machine_snapshot_t *out)
{
  if((out == 0) || (s_machine_snapshot_valid == false))
  {
    return false;
  }

  *out = s_machine_snapshot;
  return true;
}

/**
 * @brief   绑定设置页面回调函数
 *
 * @param[in] handlers  回调函数结构体指针
 *
 * 操作流程：
 *   1. 检查参数有效性
 *   2. 复制回调函数结构体
 *   3. 如果参数为NULL，清除回调函数
 */
void ui_service_bind_setting_handlers(const ui_setting_handlers_t *handlers)
{
  if(handlers == 0)
  {
    memset(&s_setting_handlers, 0, sizeof(s_setting_handlers));
    return;
  }

  s_setting_handlers = *handlers;
}

/**
 * @brief   保存DMX地址
 *
 * @param[in] value  DMX地址值
 *
 * 操作流程：
 *   1. 检查回调函数是否存在
 *   2. 调用回调函数保存地址
 */
void ui_service_save_dmx_addr(int16_t value)
{
  if(s_setting_handlers.save_dmx_addr != 0)
  {
    s_setting_handlers.save_dmx_addr(value);
  }
}

/**
 * @brief   保存DMX模式
 *
 * @param[in] value  DMX模式值
 *
 * 操作流程：
 *   1. 检查回调函数是否存在
 *   2. 调用回调函数保存模式
 */
void ui_service_save_dmx_mode(int16_t value)
{
  if(s_setting_handlers.save_dmx_mode != 0)
  {
    s_setting_handlers.save_dmx_mode(value);
  }
}

/**
 * @brief   保存点火延迟
 *
 * @param[in] value  点火延迟值
 *
 * 操作流程：
 *   1. 检查回调函数是否存在
 *   2. 调用回调函数保存延迟
 */
void ui_service_save_ign_delay(int16_t value)
{
  if(s_setting_handlers.save_ign_delay != 0)
  {
    s_setting_handlers.save_ign_delay(value);
  }
}

/**
 * @brief   保存锁定延迟
 *
 * @param[in] value  锁定延迟值
 *
 * 操作流程：
 *   1. 检查回调函数是否存在
 *   2. 调用回调函数保存延迟
 */
void ui_service_save_lock_delay(int16_t value)
{
  if(s_setting_handlers.save_lock_delay != 0)
  {
    s_setting_handlers.save_lock_delay(value);
  }
}

/**
 * @brief   保存倾斜保护启用状态
 *
 * @param[in] value  倾斜保护启用状态
 *
 * 操作流程：
 *   1. 检查回调函数是否存在
 *   2. 调用回调函数保存状态
 */
void ui_service_save_tilt_enable(int16_t value)
{
  if(s_setting_handlers.save_tilt_enable != 0)
  {
    s_setting_handlers.save_tilt_enable(value);
  }
}

/**
 * @brief   保存语言设置
 *
 * @param[in] value  语言代码
 *
 * 操作流程：
 *   1. 检查回调函数是否存在
 *   2. 调用回调函数保存语言
 */
void ui_service_save_language(int16_t value)
{
  if(s_setting_handlers.save_language != 0)
  {
    s_setting_handlers.save_language(value);
  }
}
