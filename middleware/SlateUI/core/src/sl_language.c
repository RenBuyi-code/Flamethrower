/**
 * @file    sl_language.c
 * @brief   SlateUI 多语言支持实现
 *
 * 本文件实现零 RAM 占用的多语言切换：
 *   · 语言表为 const 二维数组，存储在 Flash 中。
 *   · 切换语言仅需修改 curr_lang 索引变量。
 *   · 所有字符串通过枚举 ID 索引，编译期确定。
 *
 * 扩展方式：
 *   1. 在 sl_language.h 的字符串 ID 枚举中添加新条目；
 *   2. 在下方 strings[][] 数组中为每种语言添加对应翻译；
 *   3. 新增语言时在语言索引枚举中添加，并扩展数组行。
 */

#include "../inc/sl_language.h"

/* ======================== 当前语言状态 ======================== */

/** @brief 当前语言索引，默认英语 */
static int curr_lang = SL_LANG_EN;

/* ======================== 语言字符串表 ======================== */

/**
 * @brief  多语言字符串表（const，存储在 Flash）
 *
 * 二维数组：第一维为语言索引，第二维为字符串 ID。
 * 使用 C99 指定初始化器，未指定的条目自动初始化为 NULL。
 */
static const char* const strings[SL_LANG_MAX][SL_STR_COUNT] = {
    [SL_LANG_EN] = {
        [SL_STR_BRIGHTNESS] = "Brightness",
        [SL_STR_POWER]      = "Power",
        [SL_STR_SETTINGS]   = "Settings",
        [SL_STR_BACK]       = "Back",
        [SL_STR_LANGUAGE]   = "Language",
        [SL_STR_CN]         = "Chinese",
        [SL_STR_EN]         = "English",
        [SL_STR_DMX_SET]    = "DMX Set",
        [SL_STR_PRESSURE_SET] = "Pressure Set",
        [SL_STR_PRESS_MENU] = "Press MENU",
        [SL_STR_READY]      = "Ready",
        [SL_STR_FLT]        = "Fault",
        [SL_STR_FIRE]       = "Firing",
        [SL_STR_STOPPED]    = "Stopped",
        [SL_STR_LOCKED]     = "Locked",
        [SL_STR_MS]         = "ms",
        [SL_STR_ON]         = "ON",
        [SL_STR_OFF]        = "OFF",
        [SL_STR_ADDR]       = "Addr",
        [SL_STR_MODE]       = "Mode",
        [SL_STR_WELCOME]    = "Welcome",
        [SL_STR_IGN]        = "Ign Delay",
        [SL_STR_LOCK]       = "Lock Delay",
        [SL_STR_CHECKING]   = "System Checking...",
        [SL_STR_SAFETY_SET] = "Safety Set",
        [SL_STR_TILT]       = "Tilt",
        [SL_STR_CHARGING]   = "Charging",
        [SL_STR_E1_PRESSURE] = "E1-Pressure Fault",
        [SL_STR_E2_TILT_FAULT] = "E2-Tilt Fault",
        [SL_STR_E3_VOLTAGE] = "E3-Voltage Fault",
        [SL_STR_E4_LOCKED_FAULT] = "E4-Locked",
        [SL_STR_E5_RELIEF]  = "E5-Relief Fault",
    },
    [SL_LANG_CN] = {
        [SL_STR_BRIGHTNESS] = "亮度",
        [SL_STR_POWER]      = "电源",
        [SL_STR_SETTINGS]   = "设置",
        [SL_STR_BACK]       = "返回",
        [SL_STR_LANGUAGE]   = "语言",
        [SL_STR_CN]         = "中文",
        [SL_STR_EN]         = "English ",
        [SL_STR_DMX_SET]    = "DMX设置",
        [SL_STR_PRESSURE_SET] = "压力设置",
        [SL_STR_PRESS_MENU] = "按MENU",
        [SL_STR_READY]      = "准备就绪",
        [SL_STR_FLT]        = "故障",
        [SL_STR_FIRE]       = "喷射",
        [SL_STR_STOPPED]    = "停止",
        [SL_STR_LOCKED]     = "锁定",
        [SL_STR_MS]         = "ms",
        [SL_STR_ON]         = "开",
        [SL_STR_OFF]        = "关",
        [SL_STR_ADDR]       = "地址",
        [SL_STR_MODE]       = "模式",
        [SL_STR_WELCOME]    = "欢迎使用",
        [SL_STR_IGN]        = "点火延时",
        [SL_STR_LOCK]       = "锁阀延时",
        [SL_STR_CHECKING]   = "系统检测中...",
        [SL_STR_SAFETY_SET] = "安全设置",
        [SL_STR_TILT]       = "倾倒保护",
        [SL_STR_CHARGING]   = "正在加压",
        [SL_STR_E1_PRESSURE] = "E1-加压故障",
        [SL_STR_E2_TILT_FAULT] = "E2-机器倾倒",
        [SL_STR_E3_VOLTAGE] = "E3-电压故障",
        [SL_STR_E4_LOCKED_FAULT] = "E4-系统上锁",
        [SL_STR_E5_RELIEF]  = "E5-泄压故障",
    },
};

/* ======================== 多语言接口实现 ======================== */

/**
 * @brief  根据字符串 ID 获取当前语言的字符串
 * @param  str_id  字符串 ID（SL_STR_xxx 枚举值）
 * @retval 指向当前语言对应字符串的只读指针
 * @note   若 str_id 越界，返回 "??" 表示未知字符串
 */
const char* sl_lang_get(uint16_t str_id) {
    if (str_id < SL_STR_COUNT) {
        return strings[curr_lang][str_id];
    }
    return "??";
}

/**
 * @brief  设置当前语言
 * @param  lang_id  语言索引（SL_LANG_EN / SL_LANG_CN 等）
 * @note   仅接受有效范围内的索引，越界值被忽略；
 *         切换后需手动调用 sl_page_request_redraw() 刷新界面
 */
void sl_lang_set(int lang_id) {
    if (lang_id >= 0 && lang_id < SL_LANG_MAX) {
        curr_lang = lang_id;
    }
}

/**
 * @brief  获取当前语言索引
 * @retval 当前语言索引值（SL_LANG_EN / SL_LANG_CN 等）
 */
int sl_lang_get_current(void) {
    return curr_lang;
}
