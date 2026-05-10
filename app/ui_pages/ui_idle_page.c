/**
 * @file    ui_idle_page.c
 * @brief   空闲主页面 — 系统运行状态实时显示
 *
 * ## 显示布局（128×64 点阵 LCD，两行）
 *
 *   行 0（y=0）: "DMX:001"     ← DMX 地址
 *   行 1（y=16）: "准备就绪"     ← 系统状态文字，居中显示
 *
 * ## 状态文字切换逻辑
 *
 *   ┌─────────────┬──────────────────────────┐
 *   │ 机器状态     │ 屏幕显示                  │
 *   ├─────────────┼──────────────────────────┤
 *   │ FIRING      │ "喷火中"（SL_STR_FIRE）    │
 *   │ FAULT/LOCKED│ E1-E5 故障文字            │
 *   │ READY+建压  │ "正在加压"（SL_STR_CHARGING）│
 *   │ READY+待机  │ "准备就绪"（SL_STR_READY）  │
 *   └─────────────┴──────────────────────────┘
 *
 * ## 压力滤波
 *   4 点滑动平均滤波，每 500ms 更新一次显示值，
 *   防止 ADC 噪声导致压力数字跳动。
 */

#include "ui_idle_page.h"
#include "../ui_services.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

typedef struct
{
  ui_idle_data_t data;
  uint8_t pressure_display;
  uint8_t pressure_ring[4];
  uint8_t pressure_ring_idx;
  bool pressure_filter_inited;
  TickType_t pressure_update_tick;
} ui_idle_state_t;

static sl_Page s_idle_page;
static ui_idle_state_t s_idle_state;

static ui_idle_state_t *uidle_state(sl_Page *self)
{
  return SL_PAGE_DATA_AS(ui_idle_state_t, self);
}

/**
 * @brief   将故障掩码转换为可读的错误名称
 *
 * @param[in] fault_mask  故障位掩码（见 fault_manager.h）
 * @return    错误名称字符串指针
 *
 * 故障优先级从 E1 到 E5，取最低设置位对应的错误显示。
 * 例如：mask=0x03（E1+E2 同时存在）→ 显示 "E1-加压故障"
 */
static const char *uidle_fault_text(uint32_t fault_mask)
{
  if((fault_mask & 0x01U) != 0U)
  {
    return SL_LANG(SL_STR_E1_PRESSURE);
  }
  if((fault_mask & 0x02U) != 0U)
  {
    return SL_LANG(SL_STR_E2_TILT_FAULT);
  }
  if((fault_mask & 0x04U) != 0U)
  {
    return SL_LANG(SL_STR_E3_VOLTAGE);
  }
  if((fault_mask & 0x08U) != 0U)
  {
    return SL_LANG(SL_STR_E4_LOCKED_FAULT);
  }
  if((fault_mask & 0x10U) != 0U)
  {
    return SL_LANG(SL_STR_E5_RELIEF);
  }
  return SL_LANG(SL_STR_FLT);
}

static void idle_init(sl_Page *self)
{
  ui_idle_state_t *st = uidle_state(self);

  memset(st, 0, sizeof(*st));
  st->data.state = MACHINE_BOOT;
}

/**
 * @brief   从 UI 服务同步最新快照到页面数据
 *
 * 内部包含 4 点滑动平均压力滤波：
 * 新值覆盖环形缓冲区中最旧的值，计算四值平均后输出。
 * 滤波更新频率限制为 UI_PRESSURE_UPDATE_MS（500ms）。
 */
static void idle_sync_from_service(sl_Page *self)
{
  ui_idle_state_t *st = uidle_state(self);
  ui_machine_snapshot_t snap;
  uint8_t filtered_pct;
  TickType_t now_tick;

  if(ui_service_get_machine_snapshot(&snap) == false)
  {
    return;
  }

  /* 首次运行：填充环形缓冲区全部为当前值 */
  if(st->pressure_filter_inited == false)
  {
    uint8_t i;

    for(i = 0U; i < 4U; i++)
    {
      st->pressure_ring[i] = snap.pressure_pct;
    }

    st->pressure_ring_idx = 0U;
    st->pressure_display = snap.pressure_pct;
    st->pressure_filter_inited = true;
    filtered_pct = snap.pressure_pct;
  }
  else
  {
    uint16_t sum;
    uint8_t avg;

    /* 写入环形缓冲区，索引循环 0→1→2→3→0… */
    st->pressure_ring[st->pressure_ring_idx] = snap.pressure_pct;
    st->pressure_ring_idx =
        (uint8_t)((st->pressure_ring_idx + 1U) & 0x03U);

    sum = (uint16_t)st->pressure_ring[0]
        + (uint16_t)st->pressure_ring[1]
        + (uint16_t)st->pressure_ring[2]
        + (uint16_t)st->pressure_ring[3];
    avg = (uint8_t)((sum + 2U) / 4U);  /* +2 实现四舍五入 */
    st->pressure_display = avg;
    filtered_pct = avg;
  }

  /* 限频更新压力值，避免高频抖动刷新屏幕 */
  now_tick = xTaskGetTickCount();
  if((st->pressure_update_tick == 0U) ||
     ((now_tick - st->pressure_update_tick) >=
      pdMS_TO_TICKS(UI_PRESSURE_UPDATE_MS)))
  {
    st->data.pressure_pct = filtered_pct;
    st->pressure_update_tick = now_tick;
  }

  st->data.state = snap.state;
  st->data.dmx_online = snap.dmx_online;
  st->data.pumping = snap.pumping;
  st->data.fault_mask = snap.fault_mask;
  st->data.dmx_address = snap.dmx_addr;
  st->data.valid = true;
}

/**
 * @brief   绘制空闲页面
 *
 * 根据机器状态选择显示内容：
 *   1. FIRING → 显示 "喷火中"（优先级最高）
 *   2. FAULT/LOCKED → 显示 E1~E5 错误
 *   3. READY + 建压中 → "正在加压"
 *   4. READY + 待机 → "准备就绪"
 */
static void idle_draw(sl_Page *self)
{
  ui_idle_state_t *st = uidle_state(self);
  char row1[16];
  const char *row2;

  idle_sync_from_service(self);

  snprintf(row1, sizeof(row1), "DMX:%03u",
           (unsigned)st->data.dmx_address);

  if(st->data.state == MACHINE_FIRING)
  {
    row2 = SL_LANG(SL_STR_FIRE);        /* "喷火中" */
  }
  else if((st->data.fault_mask != 0U) ||
          (st->data.state == MACHINE_FAULT) ||
          (st->data.state == MACHINE_LOCKED))
  {
    row2 = uidle_fault_text(st->data.fault_mask);
  }
  else
  {
    row2 = (st->data.pumping == false)
             ? SL_LANG(SL_STR_READY)    /* "准备就绪" */
             : SL_LANG(SL_STR_CHARGING);/* "正在加压" */
  }

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);
  (void)sl_text_draw_center(0, row1, &sl_font_chinese, 0);
  (void)sl_text_draw_center(16, row2, &sl_font_chinese, 0);
}

static int idle_proc(sl_Page *self, const sl_Event *evt)
{
  ui_idle_state_t *st = uidle_state(self);

  /* 故障状态下不响应按键 */
  if((st->data.fault_mask != 0U) ||
     (st->data.state == MACHINE_FAULT) ||
     (st->data.state == MACHINE_LOCKED))
  {
    return 0;
  }

  /* 菜单键 → 进入主菜单 */
  if(evt->type == SL_EVT_KEY_BACK)
  {
    (void)sl_ui_navigate("main_menu");
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
    s_idle_page.tick = 0;
    s_idle_page.data = &s_idle_state;
    s_idle_page.arg = 0;
  }

  return &s_idle_page;
}
