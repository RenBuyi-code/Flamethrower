#include "ui_idle_page.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_page_manager.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

#define UI_DISP_W          122
#define UI_CHAR_W          8
#define UI_CHAR_H          16
#define UI_MAX_CHARS       15
#define UI_PRESSURE_UPDATE_MS 500U

typedef struct
{
  machine_state_t state;
  bool dmx_online;
  bool pumping;
  uint8_t pressure_pct;
  uint32_t fault_mask;
  uint16_t dmx_address;
  bool valid;
} ui_idle_data_t;

static sl_Page s_idle_page;
static ui_idle_data_t s_data;
static bool s_enter_menu;
static uint8_t s_pressure_display;
static uint8_t s_pressure_ring[4];
static uint8_t s_pressure_ring_idx;
static bool s_pressure_filter_inited;
static TickType_t s_pressure_update_tick;

static const char *uidle_state_text(machine_state_t state)
{
  bool is_cn;

  is_cn = (sl_lang_get_current() == SL_LANG_CN);

  switch(state)
  {
    case MACHINE_READY:
      return is_cn ? "就绪" : "RDY";
    case MACHINE_FIRING:
      return is_cn ? "喷射" : "FIR";
    case MACHINE_RELIEF:
      return is_cn ? "泄压" : "REL";
    case MACHINE_FAULT:
      return is_cn ? "故障" : "FLT";
    case MACHINE_LOCKED:
      return is_cn ? "上锁" : "LCK";
    case MACHINE_SELFTEST:
      return is_cn ? "检测" : "CHK";
    case MACHINE_BOOT:
    default:
      return is_cn ? "启动" : "BOT";
  }
}

static void uidle_append_fault_str(char *buf, uint16_t buf_size, uint32_t fault_mask)
{
  if((fault_mask & 0x01U) != 0U)
  {
    strncpy(buf, "E1-加压故障", (size_t)(buf_size - 1U));
  }
  else if((fault_mask & 0x02U) != 0U)
  {
    strncpy(buf, "E2-机器倾倒", (size_t)(buf_size - 1U));
  }
  else if((fault_mask & 0x04U) != 0U)
  {
    strncpy(buf, "E3-电压故障", (size_t)(buf_size - 1U));
  }
  else if((fault_mask & 0x08U) != 0U)
  {
    strncpy(buf, "E4-系统上锁", (size_t)(buf_size - 1U));
  }
  else if((fault_mask & 0x10U) != 0U)
  {
    strncpy(buf, "E5-泄压故障", (size_t)(buf_size - 1U));
  }
  else
  {
    strncpy(buf, "FAULT", (size_t)(buf_size - 1U));
  }
  buf[buf_size - 1U] = '\0';
  (void)buf_size;
}

static int uidle_center_x(const char *text)
{
  const unsigned char *p = (const unsigned char *)text;
  int w = 0;

  while(*p != '\0')
  {
    if(*p < 0x80U)
    {
      w += UI_CHAR_W;
      p += 1;
    }
    else if(((*p & 0xF0U) == 0xE0U) && (p[1] != '\0') && (p[2] != '\0'))
    {
      w += 16;
      p += 3;
    }
    else if(((*p & 0xE0U) == 0xC0U) && (p[1] != '\0'))
    {
      w += 16;
      p += 2;
    }
    else
    {
      w += 16;
      p += 1;
    }
  }

  if(w >= UI_DISP_W) return 0;
  return (UI_DISP_W - w) / 2;
}

static void idle_init(sl_Page *self)
{
  (void)self;
  s_data.state = MACHINE_BOOT;
  s_data.dmx_online = false;
  s_data.pumping = false;
  s_data.pressure_pct = 0U;
  s_data.fault_mask = 0U;
  s_data.valid = false;
  s_enter_menu = false;
  s_pressure_filter_inited = false;
  s_pressure_update_tick = 0U;
}

static void idle_draw(sl_Page *self)
{
  char row1[UI_MAX_CHARS + 1];
  char row2[UI_MAX_CHARS + 1];
  int x;

  (void)self;

  if(s_data.fault_mask != 0U)
  {
    snprintf(row1, sizeof(row1), "DMX:%03u", (unsigned)s_data.dmx_address);
    uidle_append_fault_str(row2, sizeof(row2), s_data.fault_mask);
  }
  else
  {
    snprintf(row1, sizeof(row1), "DMX:%03u", (unsigned)s_data.dmx_address);
    if(sl_lang_get_current() == SL_LANG_CN)
    {
      strncpy(row2, (s_data.pressure_pct >= 100U) ? "准备就绪" : "正在加压", sizeof(row2) - 1U);
    }
    else
    {
      strncpy(row2, (s_data.pressure_pct >= 100U) ? "Ready" : "Charging", sizeof(row2) - 1U);
    }
    row2[sizeof(row2) - 1U] = '\0';
  }

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  x = uidle_center_x(row1);
  sl_disp_draw_string((uint16_t)x, 0, row1, &sl_font_chinese, 0);

  x = uidle_center_x(row2);
  sl_disp_draw_string((uint16_t)x, 16, row2, &sl_font_chinese, 0);
}

static int idle_proc(sl_Page *self, const sl_Event *evt)
{
  (void)self;
  (void)evt;

  if((s_data.fault_mask != 0U) || (s_data.state == MACHINE_FAULT) || (s_data.state == MACHINE_LOCKED))
  {
    s_enter_menu = false;
    return 0;
  }

  if(evt->type == SL_EVT_KEY_BACK)
  {
    s_enter_menu = true;
  }
  return 0;
}

sl_Page *ui_idle_page_get(void)
{
  if(s_idle_page.init == 0)
  {
    s_idle_page.name = "idle";
    s_idle_page.init = idle_init;
    s_idle_page.draw = idle_draw;
    s_idle_page.proc = idle_proc;
    s_idle_page.exit = 0;
    s_idle_page.presenter = 0;
    s_idle_page.data = 0;
    s_idle_page.arg = 0;
  }
  return &s_idle_page;
}

bool ui_idle_page_consume_enter_menu(void)
{
  bool v = s_enter_menu;
  s_enter_menu = false;
  return v;
}

void ui_idle_page_update(machine_state_t state, bool dmx_online, bool pumping, uint8_t pressure_pct, uint32_t fault_mask, uint16_t dmx_address)
{
  bool changed;
  uint8_t filtered_pct;
  TickType_t now_tick;
  bool pressure_due;

  if(s_pressure_filter_inited == false)
  {
    uint8_t i;
    for(i = 0U; i < 4U; i++)
    {
      s_pressure_ring[i] = pressure_pct;
    }
    s_pressure_ring_idx = 0U;
    s_pressure_display = pressure_pct;
    s_pressure_filter_inited = true;
    filtered_pct = pressure_pct;
  }
  else
  {
    uint16_t sum;
    uint8_t avg;

    s_pressure_ring[s_pressure_ring_idx] = pressure_pct;
    s_pressure_ring_idx = (uint8_t)((s_pressure_ring_idx + 1U) & 0x03U);

    sum = (uint16_t)s_pressure_ring[0] + (uint16_t)s_pressure_ring[1]
        + (uint16_t)s_pressure_ring[2] + (uint16_t)s_pressure_ring[3];
    avg = (uint8_t)((sum + 2U) / 4U);

    if(avg != s_pressure_display)
    {
      s_pressure_display = avg;
    }
    filtered_pct = s_pressure_display;
  }

  now_tick = xTaskGetTickCount();
  pressure_due = (!s_data.valid) || (s_pressure_update_tick == 0U) ||
                 ((now_tick - s_pressure_update_tick) >= pdMS_TO_TICKS(UI_PRESSURE_UPDATE_MS));

  changed = (s_data.state != state) ||
            (s_data.dmx_online != dmx_online) ||
            (s_data.pumping != pumping) ||
            (s_data.fault_mask != fault_mask) ||
            (s_data.dmx_address != dmx_address);

  if(pressure_due && (s_data.pressure_pct != filtered_pct))
  {
    changed = true;
    s_data.pressure_pct = filtered_pct;
    s_pressure_update_tick = now_tick;
  }

  s_data.state = state;
  s_data.dmx_online = dmx_online;
  s_data.pumping = pumping;
  s_data.fault_mask = fault_mask;
  s_data.dmx_address = dmx_address;
  s_data.valid = true;

  if(changed)
  {
    sl_page_request_redraw();
  }
}
