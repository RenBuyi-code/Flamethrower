/**
 * @file    sl_language.h
 * @brief   SlateUI 多语言支持（字符串 ID 映射）
 *
 * 本模块实现零 RAM 占用的多语言切换机制：
 *   · 使用枚举定义字符串 ID，编译期确定，无运行时查找开销。
 *   · 语言表为 const 二维数组，存储在 Flash 中，零 RAM 占用。
 *   · 切换语言仅需修改一个索引变量，所有 SL_LANG() 调用自动生效。
 *   · 提供 SL_LANG(id) 宏，便于在绘制代码中直接获取当前语言字符串。
 *
 * 扩展方式：
 *   1. 在字符串 ID 枚举中添加新条目（SL_STR_COUNT 之前）。
 *   2. 在语言表中为每种语言添加对应的翻译字符串。
 *   3. 如需新增语言，在语言索引枚举中添加条目并扩展语言表。
 */

#ifndef SL_LANGUAGE_H
#define SL_LANGUAGE_H

#include <stdint.h>

/* ======================== 语言索引定义 ======================== */

/**
 * @brief  语言索引枚举
 *
 * 每种支持的语言对应一个索引值，用于语言表寻址。
 * 新增语言时在此枚举中添加，并在语言表中扩展对应行。
 */
enum {
    SL_LANG_EN = 0,  /**< 英语（默认语言） */
    SL_LANG_CN,      /**< 中文 */
    SL_LANG_MAX      /**< 语言数量计数器，请勿手动赋值 */
};

/* ======================== 字符串 ID 定义 ======================== */

/**
 * @brief  字符串 ID 枚举
 *
 * 每个字符串资源对应一个唯一 ID，用于从语言表中检索。
 * 新增字符串时在 SL_STR_COUNT 之前添加条目，
 * 并在语言表中为每种语言补充对应翻译。
 */
enum {
    SL_STR_NONE      = 0,  /**< 空字符串（无效 ID） */
    SL_STR_BRIGHTNESS,     /**< "亮度" / "Brightness" */
    SL_STR_POWER,          /**< "电源" / "Power" */
    SL_STR_SETTINGS,       /**< "设置" / "Settings" */
    SL_STR_BACK,           /**< "返回" / "Back" */
    SL_STR_LANGUAGE,       /**< "语言" / "Language" */
    SL_STR_CN,             /**< "中文" / "Chinese" */
    SL_STR_EN,             /**< "英文" / "English" */
    SL_STR_DMX_SET,        /**< "DMX设置" / "DMX Set" */
    SL_STR_PRESSURE_SET,   /**< "喷火设置" / "Fire Set" */
    SL_STR_PRESS_MENU,     /**< "按MENU菜单" / "Press MENU" */
    SL_STR_READY,          /**< "就绪" / "Ready" */
    SL_STR_FLT,            /**< "故障" / "Fault" */
    SL_STR_FIRE,           /**< "喷火中" / "Firing" */
    SL_STR_STOPPED,        /**< "停止" / "Stopped" */
    SL_STR_LOCKED,         /**< "锁定" / "Locked" */
    SL_STR_MS,             /**< "毫秒" / "ms" */
    SL_STR_ON,             /**< "开" / "ON" */
    SL_STR_OFF,            /**< "关" / "OFF" */
    SL_STR_ADDR,           /**< "地址" / "Addr" */
    SL_STR_MODE,           /**< "模式" / "Mode" */
    SL_STR_WELCOME,        /**< "欢迎使用" / "Welcome" */
    SL_STR_IGN,            /**< "点火延时" / "Ign Delay" */
    SL_STR_LOCK,           /**< "锁阀延时" / "Lock Delay" */
    SL_STR_CHECKING,       /**< "系统检测中" / "System Checking" */
    SL_STR_SAFETY_SET,     /**< "安全设置" / "Safety Set" */
    SL_STR_TILT,           /**< "倾斜保护" / "Tilt" */
    SL_STR_CHARGING,       /**< "正在加压" / "Charging" */
    SL_STR_E1_PRESSURE,    /**< "E1-加压故障" / "E1-Pressure Fault" */
    SL_STR_E2_TILT_FAULT,  /**< "E2-机器倾倒" / "E2-Tilt Fault" */
    SL_STR_E3_VOLTAGE,     /**< "E3-电压故障" / "E3-Voltage Fault" */
    SL_STR_E4_LOCKED_FAULT,/**< "E4-系统上锁" / "E4-Locked" */
    SL_STR_E5_RELIEF,      /**< "E5-泄压故障" / "E5-Relief Fault" */
    SL_STR_COUNT            /**< 字符串数量计数器，请勿手动赋值 */
};

/* ======================== 多语言接口 ======================== */

/**
 * @brief  根据字符串 ID 获取当前语言的字符串
 * @param  str_id  字符串 ID（SL_STR_xxx 枚举值）
 * @retval 指向当前语言对应字符串的只读指针
 * @note   若 str_id 越界，返回空字符串 ""
 */
const char* sl_lang_get(uint16_t str_id);

/**
 * @brief  设置当前语言
 * @param  lang_id  语言索引（SL_LANG_EN / SL_LANG_CN 等）
 * @note   切换后需手动调用 sl_page_request_redraw() 刷新界面
 */
void        sl_lang_set(int lang_id);

/**
 * @brief  获取当前语言索引
 * @retval 当前语言索引值（SL_LANG_EN / SL_LANG_CN 等）
 */
int         sl_lang_get_current(void);

/**
 * @brief  获取当前语言字符串的便捷宏
 * @param  id  字符串 ID（SL_STR_xxx 枚举值）
 * @note   等价于 sl_lang_get(id)，用于绘制代码中简化调用
 */
#define     SL_LANG(id) sl_lang_get(id)

#endif
