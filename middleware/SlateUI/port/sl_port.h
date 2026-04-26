/**
 * @file    sl_port.h
 * @brief   SlateUI 硬件/平台移植接口
 *
 * 本模块定义 SlateUI 与硬件平台的接口边界：
 *   · 所有平台相关代码必须集中在 sl_port.c 中实现，
 *     上层模块通过此头文件调用硬件功能。
 *   · 显示接口：设置窗口、发送像素数据（同步/异步）。
 *   · 输入接口：初始化输入源、轮询输入状态。
 *   · 输入可以是中断驱动、轮询驱动或混合模式。
 *
 * 移植步骤：
 *   1. 在 sl_port.c 中实现所有函数；
 *   2. 根据目标屏幕修改 SL_DISP_WIDTH / SL_DISP_HEIGHT；
 *   3. 根据目标 MCU 配置 GPIO/SPI/I2C/DMA；
 *   4. 在中断或轮询中调用 sl_event_post() 投递按键事件。
 */

#ifndef SL_PORT_H
#define SL_PORT_H

#include <stdint.h>

/* ======================== 显示尺寸配置 ======================== */

/**
 * @brief  显示屏宽度（像素）
 * @note   默认 128，可在编译选项中覆盖以适配不同屏幕
 */
#ifndef SL_DISP_WIDTH
#define SL_DISP_WIDTH  128
#endif

/**
 * @brief  显示屏高度（像素）
 * @note   默认 64，可在编译选项中覆盖以适配不同屏幕
 */
#ifndef SL_DISP_HEIGHT
#define SL_DISP_HEIGHT 64
#endif

/* ======================== 核心平台钩子函数 ======================== */

/**
 * @brief  平台初始化
 * @note   应在 main() 中最早调用，完成以下初始化：
 *         1) 系统时钟配置
 *         2) GPIO 初始化（按键 + 显示控制引脚）
 *         3) I2C/SPI 外设初始化
 *         4) 显示控制器初始化（如 SSD1306_Init）
 *         5) DMA 初始化（可选，用于异步发送）
 */
void sl_port_init(void);

/**
 * @brief  设置 LCD 显示窗口
 * @param  x  窗口左上角 X 坐标（像素）
 * @param  y  窗口左上角 Y 坐标（像素，页对齐）
 * @param  w  窗口宽度（像素）
 * @param  h  窗口高度（像素，页对齐，8 的倍数）
 * @note   后续 sl_hw_send_pixels() 发送的数据将写入此窗口区域
 */
void sl_hw_set_window(int x, int y, int w, int h);

/**
 * @brief  同步发送像素数据到 LCD
 * @param  data  像素数据缓冲区指针
 * @param  len   数据长度（字节）
 * @note   阻塞式发送，发送完成后才返回；
 *         适用于无 DMA 或 DMA 不可用的场景
 */
void sl_hw_send_pixels(const uint8_t *data, int len);

/* ======================== 输入源钩子函数 ======================== */

/**
 * @brief  初始化输入源
 * @note   根据输入模式配置：
 *         - 中断模式：配置 EXTI/NVIC
 *         - 轮询模式：初始化按键扫描/消抖状态
 */
void sl_port_input_init(void);

/**
 * @brief  轮询输入源（主循环中调用）
 * @note   典型流程：
 *         1) 读取按键 GPIO 状态
 *         2) 消抖处理
 *         3) 检测按下/释放边沿
 *         4) 调用 sl_event_post() 投递事件
 */
void sl_port_poll_input(void);

/**
 * @brief  启用按键中断（向后兼容接口）
 * @note   等价于 sl_port_input_init()，新代码应使用后者
 */
void sl_port_enable_key_interrupts(void);

/* ======================== 可选异步发送钩子 ======================== */

/**
 * @brief  是否启用异步发送路径
 * @note   设为 1 时，sl_disp_flush() 将尝试使用 DMA 异步发送；
 *         设为 0 时，仅使用同步发送路径；
 *         可在编译选项中覆盖
 */
#ifndef SL_PORT_USE_ASYNC_TX
#define SL_PORT_USE_ASYNC_TX 0
#endif

/**
 * @brief  启动一次异步像素数据发送
 * @param  data  像素数据缓冲区指针
 * @param  len   数据长度（字节）
 * @retval 1     异步发送已启动
 * @retval 0     异步发送不可用，调用方应回退到同步发送
 * @note   使用 DMA + 中断实现，发送完成后 sl_hw_tx_busy() 应返回 0
 */
int  sl_hw_send_pixels_async(const uint8_t *data, int len);

/**
 * @brief  查询异步发送是否忙碌
 * @retval 1  DMA 正在发送数据
 * @retval 0  空闲，可以启动新的发送
 */
int  sl_hw_tx_busy(void);

/**
 * @brief  等待异步发送完成（阻塞）
 * @note   在双缓冲模式下，刷新前需等待上一次发送完成
 */
void sl_hw_tx_wait(void);

#endif
