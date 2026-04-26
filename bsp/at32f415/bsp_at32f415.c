#include "bsp_at32f415.h"
#include "bsp_uart.h"
#include "../../project/inc/at32f415_wk_config.h"
#include "../../project/inc/at32f415_conf.h"
#include "../../cfg/system_config.h"
#include <stddef.h>
#include <string.h>

typedef struct
{
  system_params_t cached_params;
  bool has_cached_params;
} bsp_storage_ctx_t;

static bsp_storage_ctx_t s_storage_ctx;

#define PARAMS_FLASH_ADDR               0x0801F800UL
#define PARAMS_IMAGE_MAGIC              0x46545031UL /* FTP1 */
#define PARAMS_IMAGE_VERSION            0x0001U
#define PARAMS_IMAGE_SIZE               128U

typedef union
{
  struct
  {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_len;
    system_params_t params;
    uint32_t crc32;
    uint8_t reserved[PARAMS_IMAGE_SIZE - 4U - 2U - 2U - sizeof(system_params_t) - 4U];
  } s;
  uint8_t raw[PARAMS_IMAGE_SIZE];
} params_flash_image_t;

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

static void write_led_pin(gpio_type *port, uint16_t pin, bool on)
{
  /* Board LEDs are active-low: reset=on, set=off. */
  write_pin(port, pin, on ? false : true);
}

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

bool bsp_at32f415_is_user_mode(void)
{
  return hal_input_read(0, INPUT_SAFETY_LOCK);
}
