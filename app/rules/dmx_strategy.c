/**
 * @file    dmx_strategy.c
 * @brief   DMX512 控制策略 — 领域层实现
 *
 * ## 核心设计
 *   本模块采用"泄压优先"策略：
 *   泄压请求（relif）具有最高优先级，一旦满足条件即响应，
 *   忽略同帧中的喷射请求。
 *
 *   这是出于安全考虑：泄压是紧急安全动作，必须无延迟响应。
 *
 * ## 模式通道（mode_ch）语义
 *   mode_ch 用于传达机器状态控制指令：
 *   - 0~49（极低值）   → 泄压请求（relif）
 *   - 50~200（中间值） → 正常火力模式
 *   - 201~255（极高值）→ 泄压请求（relif）
 *
 *   这种区间设计的好处是：即使信号线有噪声或部分通道损坏，
 *   只要噪声不超过中间区间，就不会误触发泄压。
 *
 * ## 喷射时长计算（6CH 模式）
 *   - time_ch == 0     → 0ms（无限喷射，由外部定时器控制关闭）
 *   - time_ch == 255   → 0ms（无限喷射，同上）
 *   - time_ch == 1~254 → time_ch × 10ms（如 100 = 1000ms 喷射）
 *
 * ## 安全约束
 *   - start_addr 最小为 1（DMX512 起始地址从 1 开始，非 0）
 *   - 2CH 模式最多使用 511 地址（留一个给 mode_ch）
 *   - 6CH 模式最多使用 507 地址（留 5 个给其他通道）
 */

#include "dmx_strategy.h"

/**
 * @brief   判断 mode_ch 是否处于泄压窗口
 *
 * @param[in] value  mode_ch 的 DMX 值（0~255）
 * @return    是否为泄压请求
 *
 * ## 设计原理
 *   泄压窗口分为两段：
 *   - 低段：0~49（靠近 0 端）
 *   - 高段：201~255（靠近 255 端）
 *   中间段 50~200 视为正常火力模式。
 *
 *   这种双区间设计避免单点故障（如导线接触不良导致读数为中间值）。
 */
static bool is_relief_window(uint8_t value)
{
  if(value <= 49U)
  {
    return true;
  }
  if(value >= 201U)
  {
    return true;
  }
  return false;
}

/**
 * @brief   获取指定 DMX 模式下的最大起始地址
 *
 * @param[in] mode  DMX 工作模式
 * @return    最大允许的起始地址
 *
 * @note    2CH 模式：基址 + 1（mode_ch） ≤ 512 → max = 511
 *          6CH 模式：基址 + 5（最后一个通道） ≤ 512 → max = 507
 */
uint16_t dmx_strategy_get_max_start_address(dmx_mode_t mode)
{
  if(mode == DMX_MODE_6CH)
  {
    return 507U;
  }
  if(mode == DMX_MODE_2CH)
  {
    return 511U;
  }
  return 1U;
}

/**
 * @brief   验证起始地址是否合法
 *
 * @param[in] mode        DMX 工作模式
 * @param[in] start_addr  待验证的起始地址
 * @return    地址是否合法
 *
 * @note    起始地址不能小于 1，且不能超过模式允许的最大值
 */
bool dmx_strategy_is_valid_start_address(dmx_mode_t mode, uint16_t start_addr)
{
  if(start_addr < 1U)
  {
    return false;
  }
  return (start_addr <= dmx_strategy_get_max_start_address(mode));
}

/**
 * @brief   将 DMX 通道数据转换为执行意向
 *
 * @param[in]     mode       DMX 工作模式（2CH 或 6CH）
 * @param[in]     channels   指向 DMX 帧通道数组的指针
 * @param[in]     start_addr DMX 起始地址（1~512）
 * @param[out]    intent     输出的执行意向结构
 * @return         转换是否成功
 *
 * ## 2CH 模式逻辑
 *   1. 读取 base（fire_ch）和 base+1（mode_ch）
 *   2. mode_ch in 泄压窗口？ → request_relief=true，request_fire=false
 *   3. 否则 fire_ch >= 254？ → request_fire=true
 *
 * ## 6CH 模式逻辑
 *   1. 读取 base+2（fire_ch）、base+3（time_ch）、base+5（mode_ch）
 *   2. mode_ch in 泄压窗口？ → request_relief=true，request_fire=false
 *   3. 否则 fire_ch >= 254？ → request_fire=true
 *   4. time_ch 计算：0/255 → 无限（0ms），1~254 → time×10ms
 */
bool dmx_strategy_build_intent(
  dmx_mode_t mode,
  const uint8_t *channels,
  uint16_t start_addr,
  dmx_intent_t *intent)
{
  uint16_t base;
  uint8_t fire_ch;
  uint8_t mode_ch;
  uint8_t time_ch;

  /* 参数合法性检查 */
  if((channels == 0) || (intent == 0) || (dmx_strategy_is_valid_start_address(mode, start_addr) == false))
  {
    return false;
  }

  /* 初始化 intent（所有标志清零）*/
  intent->request_fire = false;
  intent->request_relief = false;
  intent->fire_duration_ms = 0U;

  /* base 为数组索引（从 0 开始），start_addr 从 1 开始 */
  base = (uint16_t)(start_addr - 1U);

  /* ---- 2CH 模式 ---- */
  if(mode == DMX_MODE_2CH)
  {
    fire_ch = channels[base];
    mode_ch = channels[(uint16_t)(base + 1U)];

    /* 泄压优先：一旦 mode_ch 处于泄压窗口，立即响应 */
    if(is_relief_window(mode_ch))
    {
      intent->request_relief = true;
      intent->request_fire = false;
      return true;
    }

    /* 喷射请求：fire_ch >= 254 时触发 */
    intent->request_fire = (fire_ch >= 254U);
    intent->fire_duration_ms = 0U;   /* 2CH 模式不支持时长控制 */
    return true;
  }

  /* ---- 6CH 模式 ---- */
  if(mode == DMX_MODE_6CH)
  {
    fire_ch = channels[(uint16_t)(base + 2U)];
    time_ch = channels[(uint16_t)(base + 3U)];
    mode_ch = channels[(uint16_t)(base + 5U)];

    /* 泄压优先（同 2CH）*/
    if(is_relief_window(mode_ch))
    {
      intent->request_relief = true;
      intent->request_fire = false;
      return true;
    }

    /* 喷射请求 */
    intent->request_fire = (fire_ch >= 254U);

    /* 喷射时长计算：0 和 255 均表示无限时长 */
    if((time_ch == 0U) || (time_ch == 255U))
    {
      intent->fire_duration_ms = 0U;
    }
    else
    {
      intent->fire_duration_ms = (uint16_t)time_ch * 10U;  /* 1~254 → 10ms~2540ms */
    }
    return true;
  }

  return false;
}
