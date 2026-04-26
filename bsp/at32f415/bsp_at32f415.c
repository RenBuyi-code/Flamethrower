/**
 * @file    bsp_at32f415.c
 * @brief   AT32F415硬件抽象层实现
 *
 * 硬件抽象层模块，负责：
 *   - 提供统一的硬件接口（ADC、GPIO、执行器、存储等）
 *   - 参数Flash存储和加载（带CRC校验）
 *   - 硬件初始化和绑定
 *
 * 设计思路：
 *   - 通过函数指针绑定实现硬件解耦
 *   - 提供同步的ADC读取接口
 *   - 提供GPIO输入读取接口
 *   - 提供执行器输出控制接口
 *   - 提供DMX字节轮询接口
 *   - 提供参数存储和加载接口（带CRC校验）
 *   - 与其他模块的关系：
 *     - app_core：通过bsp_hal_bundle_t使用硬件接口
 *     - bsp_uart：提供DMX串口事件
 */

#include "bsp_at32f415.h"
#include "bsp_uart.h"
#include "../../project/inc/at32f415_wk_config.h"
#include "../../project/inc/at32f415_conf.h"
#include "../../cfg/system_config.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief   存储上下文结构体
 *
 * 用于管理参数缓存
 */
typedef struct
{
  /** @brief 缓存的系统参数 */
  system_params_t cached_params;
  /** @brief 是否有缓存的参数 */
  bool has_cached_params;
} bsp_storage_ctx_t;

/** @brief 存储上下文单例 */
static bsp_storage_ctx_t s_storage_ctx;

/**
 * @brief   参数Flash地址
 *
 * AT32F415的Flash最后一页用于存储参数
 */
#define PARAMS_FLASH_ADDR               0x0801F800UL
/** @brief 参数镜像魔术字（"FTP1"） */
#define PARAMS_IMAGE_MAGIC              0x46545031UL
/** @brief 参数镜像版本 */
#define PARAMS_IMAGE_VERSION            0x0001U
/** @brief 参数镜像大小（128字节） */
#define PARAMS_IMAGE_SIZE               128U

/**
 * @brief   参数Flash镜像结构体
 *
 * 定义参数存储格式，包含魔术字、版本、参数和CRC校验
 */
typedef union
{
  /** @brief 结构体形式 */
  struct
  {
    uint32_t magic;           /**< 魔术字 */
    uint16_t version;         /**< 版本号 */
    uint16_t payload_len;     /**< 负载长度 */
    system_params_t params;   /**< 系统参数 */
    uint32_t crc32;           /**< CRC32校验 */
    uint8_t reserved[PARAMS_IMAGE_SIZE - 4U - 2U - 2U - sizeof(system_params_t) - 4U];  /**< 保留区域 */
  } s;
  /** @brief 原始字节形式 */
  uint8_t raw[PARAMS_IMAGE_SIZE];
} params_flash_image_t;

/**
 * @brief   计算CRC32校验和
 *
 * @param[in] data  数据指针
 * @param[in] len   数据长度
 * @return    CRC32校验和
 *
 * 使用标准CRC32算法（ISO 3309）
 */
static uint32_t params_crc32_calc(const uint8_t *data, uint32_t len)
{
  uint32_t crc;
  uint32_t i;
  uint32_t j;
  crc = 0xFFFFFFFFUL;
  for(i = 0U; i < len; ++i)
  {
    crc ^= (uint32_t)data[i];
    for(j = 0U; j < 8U; ++j)
    {
      if((crc & 1UL) != 0UL)
      {
        crc = (crc >> 1U) ^ 0xEDB88320UL;
      }
      else
      {
        crc >>= 1U;
      }
    }
  }
  return ~crc;
}

/**
 * @brief   从Flash读取参数镜像
 *
 * @param[out] img  参数镜像指针
 * @return    是否读取成功
 *
 * 操作流程：
 *   1. 检查参数有效性
 *   2. 从Flash地址复制数据
 */
static bool params_flash_read_image(params_flash_image_t *img)
{
  const uint8_t *src;
  if(img == 0)
  {
    return false;
  }
  src = (const uint8_t *)PARAMS_FLASH_ADDR;
  memcpy(img->raw, src, PARAMS_IMAGE_SIZE);
  return true;
}

/**
 * @brief   验证参数镜像有效性
 *
 * @param[in] img  参数镜像指针
 * @return    是否有效
 *
 * 验证流程：
 *   1. 检查魔术字
 *   2. 检查版本号
 *   3. 检查负载长度
 *   4. 检查CRC校验
 */
static bool params_flash_validate(const params_flash_image_t *img)
{
  uint32_t calc;
  uint32_t crc_len;
  if(img == 0)
  {
    return false;
  }
  if(img->s.magic != PARAMS_IMAGE_MAGIC)
  {
    return false;
  }
  if(img->s.version != PARAMS_IMAGE_VERSION)
  {
    return false;
  }
  if(img->s.payload_len != (uint16_t)sizeof(system_params_t))
  {
    return false;
  }
  crc_len = (uint32_t)offsetof(params_flash_image_t, s.crc32);
  calc = params_crc32_calc(img->raw, crc_len);
  if(calc != img->s.crc32)
  {
    return false;
  }
  return true;
}

/**
 * @brief   写入参数镜像到Flash
 *
 * @param[in] img  参数镜像指针
 * @return    是否写入成功
 *
 * 操作流程：
 *   1. 解锁Flash
 *   2. 擦除扇区
 *   3. 按字写入数据
 *   4. 锁定Flash
 */
static bool params_flash_write_image(const params_flash_image_t *img)
{
  flash_status_type st;
  uint32_t addr;
  uint32_t i;
  uint32_t w;
  if(img == 0)
  {
    return false;
  }

  flash_unlock();
  flash_flag_clear(FLASH_PRGMERR_FLAG | FLASH_EPPERR_FLAG | FLASH_ODF_FLAG);

  st = flash_sector_erase(PARAMS_FLASH_ADDR);
  if(st != FLASH_OPERATE_DONE)
  {
    flash_lock();
    return false;
  }

  addr = PARAMS_FLASH_ADDR;
  for(i = 0U; i < PARAMS_IMAGE_SIZE; i += 4U)
  {
    w = ((uint32_t)img->raw[i + 0U] << 0U)
      | ((uint32_t)img->raw[i + 1U] << 8U)
      | ((uint32_t)img->raw[i + 2U] << 16U)
      | ((uint32_t)img->raw[i + 3U] << 24U);
    st = flash_word_program(addr, w);
    if(st != FLASH_OPERATE_DONE)
    {
      flash_lock();
      return false;
    }
    addr += 4U;
  }

  flash_lock();
  return true;
}

/**
 * @brief   读取ADC通道值
 *
 * @param[in] ch  ADC通道号
 * @return    ADC原始值
 *
 * 操作流程：
 *   1. 配置ADC通道
 *   2. 软件触发转换
 *   3. 等待转换完成
 *   4. 读取转换结果
 */
static uint16_t adc_channel_read(adc_channel_select_type ch)
{
  uint32_t timeout;
  adc_ordinary_channel_set(ADC1, ch, 1U, ADC_SAMPLETIME_239_5);
  adc_flag_clear(ADC1, ADC_CCE_FLAG);
  adc_ordinary_software_trigger_enable(ADC1, TRUE);
  timeout = 80000U;
  while((adc_flag_get(ADC1, ADC_CCE_FLAG) == RESET) && (timeout > 0U))
  {
    timeout--;
  }
  if(timeout == 0U)
  {
    return 0U;
  }
  return adc_ordinary_conversion_data_get(ADC1);
}

/**
 * @brief   HAL ADC读取原始值
 *
 * @param[in] ctx  上下文（未使用）
 * @param[in] id   传感器ID
 * @return    ADC原始值
 *
 * 传感器通道映射：
 *   - SENSOR_PRESSURE：ADC_CHANNEL_0
 *   - SENSOR_POWER1：ADC_CHANNEL_4
 *   - SENSOR_POWER2：ADC_CHANNEL_5
 *   - SENSOR_POWER3：ADC_CHANNEL_6
 */
static uint16_t hal_adc_read_raw(void *ctx, sensor_id_t id)
{
  (void)ctx;
  switch(id)
  {
    case SENSOR_PRESSURE:
      return adc_channel_read(ADC_CHANNEL_0);
    case SENSOR_POWER1:
      return adc_channel_read(ADC_CHANNEL_4);
    case SENSOR_POWER2:
      return adc_channel_read(ADC_CHANNEL_5);
    case SENSOR_POWER3:
      return adc_channel_read(ADC_CHANNEL_6);
    default:
      return 0U;
  }
}

/**
 * @brief   HAL GPIO输入读取
 *
 * @param[in] ctx  上下文（未使用）
 * @param[in] id   输入ID
 * @return    输入状态（true=高, false=低）
 *
 * 输入通道映射：
 *   - INPUT_SAFETY_LOCK：无线安全锁输入
 *   - INPUT_TILT_SWITCH：倾斜开关输入
 *   - INPUT_KEY_MENU：菜单键
 *   - INPUT_KEY_DOWN：下键
 *   - INPUT_KEY_UP：上键
 *   - INPUT_KEY_ENTER：确认键
 */
static bool hal_input_read(void *ctx, input_id_t id)
{
  flag_status v;
  (void)ctx;
  switch(id)
  {
    case INPUT_SAFETY_LOCK:
      v = gpio_input_data_bit_read(IO_IN_WIRELESS_GPIO_PORT, IO_IN_WIRELESS_PIN);
      return (v == SET);
    case INPUT_TILT_SWITCH:
      v = gpio_input_data_bit_read(IO_IN_TILT_SW_GPIO_PORT, IO_IN_TILT_SW_PIN);
      return (v == SET);
    case INPUT_KEY_MENU:
      v = gpio_input_data_bit_read(KEY_MENU_GPIO_PORT, KEY_MENU_PIN);
      return (v == RESET);
    case INPUT_KEY_DOWN:
      v = gpio_input_data_bit_read(KEY_DOWN_GPIO_PORT, KEY_DOWN_PIN);
      return (v == RESET);
    case INPUT_KEY_UP:
      v = gpio_input_data_bit_read(KEY_UP_GPIO_PORT, KEY_UP_PIN);
      return (v == RESET);
    case INPUT_KEY_ENTER:
      v = gpio_input_data_bit_read(KEY_ENTER_GPIO_PORT, KEY_ENTER_PIN);
      return (v == RESET);
    default:
      return false;
  }
}

/**
 * @brief   写入GPIO引脚
 *
 * @param[in] port  GPIO端口
 * @param[in] pin   GPIO引脚
 * @param[in] high  是否写入高电平
 */
static void write_pin(gpio_type *port, uint16_t pin, bool high)
{
  if(high)
  {
    gpio_bits_set(port, pin);
  }
  else
  {
    gpio_bits_reset(port, pin);
  }
}

/**
 * @brief   写入LED引脚（板载LED为低电平有效）
 *
 * @param[in] port  GPIO端口
 * @param[in] pin   GPIO引脚
 * @param[in] on    是否点亮
 *
 * @note    板载LED是低电平有效，所以on=true时写入低电平
 */
static void write_led_pin(gpio_type *port, uint16_t pin, bool on)
{
  write_pin(port, pin, on ? false : true);
}

/**
 * @brief   HAL执行器应用输出
 *
 * @param[in] ctx  上下文（未使用）
 * @param[in] out  执行器输出指针
 *
 * 输出控制：
 *   - 油泵
 *   - 油路锁定阀
 *   - 泄压阀
 *   - 点火器
 *   - 各种LED指示灯
 */
static void hal_actuator_apply(void *ctx, const actuator_output_t *out)
{
  (void)ctx;
  if(out == 0)
  {
    return;
  }

  write_pin(DRV_OIL_PUMP_GPIO_PORT, DRV_OIL_PUMP_PIN, out->oil_pump_on);
  write_pin(DRV_OIL_LOCK_SV_GPIO_PORT, DRV_OIL_LOCK_SV_PIN, out->oil_lock_valve_on);
  write_pin(DRV_RV_GPIO_PORT, DRV_RV_PIN, out->relief_valve_on);
  write_pin(DRV_IGNITER_GPIO_PORT, DRV_IGNITER_PIN, out->igniter_on);

  write_led_pin(LED_ERR_GPIO_PORT, LED_ERR_PIN, out->led_error_on);
  write_led_pin(LED_OIL_PUMP_GPIO_PORT, LED_OIL_PUMP_PIN, out->led_oil_pump_on);
  write_led_pin(LED_DMX_GPIO_PORT, LED_DMX_PIN, out->led_dmx_on);
  write_led_pin(LED_POWER_GPIO_PORT, LED_POWER_PIN, out->led_power_on);
  write_led_pin(O_LED_GPIO_PORT, O_LED_PIN, out->led_mode_on);
}

/**
 * @brief   HAL DMX字节轮询
 *
 * @param[in] ctx        上下文（未使用）
 * @param[out] byte      输出的字节
 * @param[out] is_break  是否为break信号
 * @return    是否成功获取字节
 *
 * 从DMX串口FIFO中获取事件
 */
static bool hal_dmx_poll_byte(void *ctx, uint8_t *byte, bool *is_break)
{
  bsp_uart_dmx_event_t evt;
  (void)ctx;

  if((byte == 0) || (is_break == 0))
  {
    return false;
  }

  if(bsp_uart_dmx_poll_event(&evt) == false)
  {
    return false;
  }

  *byte = evt.byte;
  *is_break = evt.is_break;
  return true;
}

/**
 * @brief   HAL存储加载参数
 *
 * @param[in] ctx  上下文（未使用）
 * @param[out] out 输出的参数指针
 * @return    是否加载成功
 *
 * 操作流程：
 *   1. 检查是否有缓存
 *   2. 从Flash读取镜像
 *   3. 验证镜像有效性
 *   4. 校验和修正参数
 *   5. 缓存参数
 */
static bool hal_storage_load(void *ctx, system_params_t *out)
{
  bsp_storage_ctx_t *st;
  params_flash_image_t img;
  (void)ctx;
  if(out == 0)
  {
    return false;
  }

  st = &s_storage_ctx;
  if(st->has_cached_params == true)
  {
    *out = st->cached_params;
    return true;
  }

  if(params_flash_read_image(&img) == false)
  {
    return false;
  }
  if(params_flash_validate(&img) == false)
  {
    return false;
  }

  st->cached_params = img.s.params;
  cfg_sanitize_params(&st->cached_params);
  st->has_cached_params = true;
  *out = st->cached_params;
  return true;
}

/**
 * @brief   HAL存储保存参数
 *
 * @param[in] ctx  上下文（未使用）
 * @param[in] in   输入的参数指针
 * @return    是否保存成功
 *
 * 操作流程：
 *   1. 检查参数有效性
 *   2. 校验和修正参数
 *   3. 构建镜像结构
 *   4. 计算CRC校验
 *   5. 写入Flash
 *   6. 更新缓存
 */
static bool hal_storage_save(void *ctx, const system_params_t *in)
{
  bsp_storage_ctx_t *st;
  params_flash_image_t img;
  system_params_t tmp;
  uint32_t crc_len;
  (void)ctx;
  if(in == 0)
  {
    return false;
  }

  tmp = *in;
  cfg_sanitize_params(&tmp);

  memset(&img, 0xFF, sizeof(img));
  img.s.magic = PARAMS_IMAGE_MAGIC;
  img.s.version = PARAMS_IMAGE_VERSION;
  img.s.payload_len = (uint16_t)sizeof(system_params_t);
  img.s.params = tmp;
  crc_len = (uint32_t)offsetof(params_flash_image_t, s.crc32);
  img.s.crc32 = params_crc32_calc(img.raw, crc_len);

  if(params_flash_write_image(&img) == false)
  {
    return false;
  }

  st = &s_storage_ctx;
  st->cached_params = tmp;
  st->has_cached_params = true;
  return true;
}

/**
 * @brief   绑定硬件抽象层接口
 *
 * @param[out] bundle  HAL绑定结构体指针
 *
 * 绑定所有硬件接口函数指针：
 *   - ADC接口
 *   - GPIO输入接口
 *   - 执行器输出接口
 *   - DMX轮询接口
 *   - 存储接口
 */
void bsp_at32f415_bind(bsp_hal_bundle_t *bundle)
{
  if(bundle == 0)
  {
    return;
  }

  bundle->adc.ctx = 0;
  bundle->adc.read_raw = hal_adc_read_raw;

  bundle->input.ctx = 0;
  bundle->input.read = hal_input_read;

  bundle->actuator.ctx = 0;
  bundle->actuator.apply = hal_actuator_apply;

  bundle->dmx.ctx = 0;
  bundle->dmx.poll_byte = hal_dmx_poll_byte;

  bundle->storage.ctx = 0;
  bundle->storage.load_params = hal_storage_load;
  bundle->storage.save_params = hal_storage_save;

  s_storage_ctx.has_cached_params = false;
  cfg_get_default_params(&s_storage_ctx.cached_params);
}

/**
 * @brief   检查是否为用户模式
 *
 * @return    用户模式状态
 *
 * 读取安全锁输入状态
 */
bool bsp_at32f415_is_user_mode(void)
{
  return hal_input_read(0, INPUT_SAFETY_LOCK);
}
