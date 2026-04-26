#include "task_ui.h"
#include "../ui_services.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../../domain/dmx_strategy.h"
#include "../../middleware/MultiButton/multi_button.h"
#define SL_PAGE_TRANSITION_MS 0
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/core/inc/sl_page_registry.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/font/sl_font.h"
#include "../../middleware/SlateUI/port/sl_port.h"
#include "../../app/ui_pages/ui_idle_page.h"
#include "../../app/ui_pages/ui_main_menu.h"
#include "../../app/ui_pages/ui_safety_page.h"
#include "../../app/ui_pages/ui_setting_page.h"
#include "../../app/ui_pages/ui_language_page.h"
#include "../../app/ui_pages/ui_splash_page.h"
#include "../../app/ui_pages/ui_checking_page.h"
#include <string.h>

enum
{
  UI_BTN_ID_MENU = 1,
  UI_BTN_ID_DOWN = 2,
  UI_BTN_ID_UP = 3,
  UI_BTN_ID_ENTER = 4
};

typedef struct
{
  Button key_menu;
  Button key_down;
  Button key_up;
  Button key_enter;
  bool initialized;
} ui_button_ctx_t;

static app_task_ui_cfg_t s_task_ui_cfg;
static ui_button_ctx_t s_ui_btn;
static int16_t s_shadow_dmx_addr;
static int16_t s_shadow_dmx_mode;
static int16_t s_shadow_ign_delay;
static int16_t s_shadow_lock_delay;
static int16_t s_shadow_tilt_enable;
static bool s_ui_pages_registered;

static const sl_PageEntry s_ui_pages[] =
{
  { "splash", ui_splash_page_get },
  { "checking", ui_checking_page_get },
  { "idle", ui_idle_page_get },
  { "main_menu", ui_main_menu_get },
  { "dmx_set", ui_setting_page_get_dmx },
  { "pressure_set", ui_setting_page_get_pressure },
  { "safety", ui_safety_page_get },
  { "language", ui_language_page_get }
};

static void task_ui_commit_params(void)
{
  if((s_task_ui_cfg.app == 0) || (s_task_ui_cfg.commit_params == 0))
  {
    return;
  }

  cfg_sanitize_params(&s_task_ui_cfg.app->params);
  s_task_ui_cfg.commit_params();
}

static void task_ui_save_dmx_addr(int16_t value)
{
  s_task_ui_cfg.app->params.dmx_address = (uint16_t)value;
  APP_LOGI("dmx addr=%u", (unsigned)s_task_ui_cfg.app->params.dmx_address);
  task_ui_commit_params();
}

static void task_ui_save_dmx_mode(int16_t value)
{
  s_task_ui_cfg.app->params.dmx_mode = (value == 1) ? DMX_MODE_6CH : DMX_MODE_2CH;
  APP_LOGI("dmx mode=%u", (unsigned)s_task_ui_cfg.app->params.dmx_mode);
  task_ui_commit_params();
}

static void task_ui_save_ign_delay(int16_t value)
{
  s_task_ui_cfg.app->params.igniter_delay_ms = (uint16_t)value;
  APP_LOGI("ign delay=%u", (unsigned)s_task_ui_cfg.app->params.igniter_delay_ms);
  task_ui_commit_params();
}

static void task_ui_save_lock_delay(int16_t value)
{
  s_task_ui_cfg.app->params.oil_lock_delay_ms = (uint16_t)value;
  APP_LOGI("lock delay=%u", (unsigned)s_task_ui_cfg.app->params.oil_lock_delay_ms);
  task_ui_commit_params();
}

static void task_ui_save_tilt_enable(int16_t value)
{
  s_task_ui_cfg.app->params.tilt_protect_enable = (value != 0) ? true : false;
  APP_LOGI("tilt protect=%u", (unsigned)s_task_ui_cfg.app->params.tilt_protect_enable);
  task_ui_commit_params();
}

static void task_ui_save_language(int16_t value)
{
  s_task_ui_cfg.app->params.language = (uint8_t)value;
  APP_LOGI("language=%u", (unsigned)s_task_ui_cfg.app->params.language);
  task_ui_commit_params();
}

static uint8_t task_ui_button_level_read(uint8_t button_id)
{
  switch(button_id)
  {
    case UI_BTN_ID_MENU:
      return s_task_ui_cfg.app->hal.input.read(s_task_ui_cfg.app->hal.input.ctx, INPUT_KEY_MENU) ? 1U : 0U;
    case UI_BTN_ID_DOWN:
      return s_task_ui_cfg.app->hal.input.read(s_task_ui_cfg.app->hal.input.ctx, INPUT_KEY_DOWN) ? 1U : 0U;
    case UI_BTN_ID_UP:
      return s_task_ui_cfg.app->hal.input.read(s_task_ui_cfg.app->hal.input.ctx, INPUT_KEY_UP) ? 1U : 0U;
    case UI_BTN_ID_ENTER:
      return s_task_ui_cfg.app->hal.input.read(s_task_ui_cfg.app->hal.input.ctx, INPUT_KEY_ENTER) ? 1U : 0U;
    default:
      return 0U;
  }
}

static void task_ui_register_pages_once(void)
{
  if(s_ui_pages_registered)
  {
    return;
  }

  sl_ui_registry_reset();
  s_ui_pages_registered = sl_ui_register_pages(s_ui_pages, (uint8_t)(sizeof(s_ui_pages) / sizeof(s_ui_pages[0])));
}

static void task_ui_btn_click_cb(Button *btn, void *user_data)
{
  (void)user_data;
  if(btn == 0)
  {
    return;
  }

  APP_LOGI("key btn_id=%u", (unsigned)btn->button_id);

  switch(btn->button_id)
  {
    case UI_BTN_ID_MENU:
      sl_ui_post_key(SL_UI_KEY_BACK, true);
      break;
    case UI_BTN_ID_DOWN:
      sl_ui_post_key(SL_UI_KEY_DOWN, true);
      break;
    case UI_BTN_ID_UP:
      sl_ui_post_key(SL_UI_KEY_UP, true);
      break;
    case UI_BTN_ID_ENTER:
      sl_ui_post_key(SL_UI_KEY_ENTER, true);
      break;
    default:
      break;
  }
}

static void task_ui_btn_repeat_cb(Button *btn, void *user_data)
{
  (void)user_data;
  if(btn == 0)
  {
    return;
  }

  if(btn->button_id == UI_BTN_ID_DOWN)
  {
    sl_ui_post_key(SL_UI_KEY_DOWN, true);
  }
  else if(btn->button_id == UI_BTN_ID_UP)
  {
    sl_ui_post_key(SL_UI_KEY_UP, true);
  }
}

static void task_ui_setup_once(void)
{
  ui_setting_handlers_t handlers;

  if((s_task_ui_cfg.app == 0) || s_ui_btn.initialized)
  {
    return;
  }

  s_shadow_dmx_addr = (int16_t)s_task_ui_cfg.app->params.dmx_address;
  s_shadow_dmx_mode = (int16_t)(s_task_ui_cfg.app->params.dmx_mode == DMX_MODE_6CH ? 1 : 0);
  s_shadow_ign_delay = (int16_t)s_task_ui_cfg.app->params.igniter_delay_ms;
  s_shadow_lock_delay = (int16_t)s_task_ui_cfg.app->params.oil_lock_delay_ms;
  s_shadow_tilt_enable = s_task_ui_cfg.app->params.tilt_protect_enable ? 1 : 0;

  ui_setting_page_set_dmx_refs(&s_shadow_dmx_addr, &s_shadow_dmx_mode);
  ui_setting_page_set_pressure_refs(&s_shadow_ign_delay, &s_shadow_lock_delay);
  ui_safety_page_set_tilt_ref(&s_shadow_tilt_enable);

  handlers.save_dmx_addr = task_ui_save_dmx_addr;
  handlers.save_dmx_mode = task_ui_save_dmx_mode;
  handlers.save_ign_delay = task_ui_save_ign_delay;
  handlers.save_lock_delay = task_ui_save_lock_delay;
  handlers.save_tilt_enable = task_ui_save_tilt_enable;
  handlers.save_language = task_ui_save_language;
  ui_service_bind_setting_handlers(&handlers);

  sl_port_init();
  sl_port_input_init();

  {
    uint8_t m = task_ui_button_level_read(UI_BTN_ID_MENU);
    uint8_t d = task_ui_button_level_read(UI_BTN_ID_DOWN);
    uint8_t u = task_ui_button_level_read(UI_BTN_ID_UP);
    uint8_t e = task_ui_button_level_read(UI_BTN_ID_ENTER);
    APP_LOGI("btn init state M=%u D=%u U=%u E=%u", (unsigned)m, (unsigned)d, (unsigned)u, (unsigned)e);
  }

  button_init(&s_ui_btn.key_menu, task_ui_button_level_read, 1U, UI_BTN_ID_MENU);
  button_init(&s_ui_btn.key_down, task_ui_button_level_read, 1U, UI_BTN_ID_DOWN);
  button_init(&s_ui_btn.key_up, task_ui_button_level_read, 1U, UI_BTN_ID_UP);
  button_init(&s_ui_btn.key_enter, task_ui_button_level_read, 1U, UI_BTN_ID_ENTER);

  button_attach(&s_ui_btn.key_menu, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_down, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_down, BTN_LONG_PRESS_HOLD, task_ui_btn_repeat_cb, 0);
  button_attach(&s_ui_btn.key_up, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_up, BTN_LONG_PRESS_HOLD, task_ui_btn_repeat_cb, 0);
  button_attach(&s_ui_btn.key_enter, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);

  (void)button_start(&s_ui_btn.key_menu);
  (void)button_start(&s_ui_btn.key_down);
  (void)button_start(&s_ui_btn.key_up);
  (void)button_start(&s_ui_btn.key_enter);

  sl_lang_set((int)s_task_ui_cfg.app->params.language);
  sl_disp_init();
  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);
  sl_disp_flush();
  task_ui_register_pages_once();
  sl_ui_init("splash");

  s_ui_btn.initialized = true;
  APP_LOGI("ui setup done");
}

void app_task_ui_init(const app_task_ui_cfg_t *cfg)
{
  if(cfg == 0)
  {
    memset(&s_task_ui_cfg, 0, sizeof(s_task_ui_cfg));
    return;
  }

  s_task_ui_cfg = *cfg;
}

void ui_task(void *pvParameters)
{
  (void)pvParameters;
  task_ui_setup_once();

  for(;;)
  {
    uint8_t tick_i;
    const char *page_name;
    EventBits_t bits;
    uint16_t pressure_raw;
    uint8_t pressure_pct;
    bool dmx_online;
    machine_state_t st;
    ui_machine_snapshot_t snap;
    bool snapshot_changed;
    actuator_status_t act_st;
    BaseType_t act_ok;
    bool pumping;

    button_ticks();

    bits = xEventGroupGetBits(s_task_ui_cfg.event_group);
    pressure_raw = s_task_ui_cfg.app->hal.adc.read_raw(s_task_ui_cfg.app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    dmx_online = ((bits & s_task_ui_cfg.dmx_online_bit) != 0U);
    st = s_task_ui_cfg.app->machine.current;

    act_ok = xQueuePeek(s_task_ui_cfg.q_actuator_status, &act_st, 0);
    pumping = ((act_ok == pdTRUE) && act_st.out.oil_pump_on && (act_st.fire_active == false) && (act_st.relief_active == false));
    snap.state = st;
    snap.pressure_pct = pressure_pct;
    snap.dmx_addr = s_task_ui_cfg.app->params.dmx_address;
    snap.fault_mask = s_task_ui_cfg.app->faults.latched_mask;
    snap.dmx_online = dmx_online;
    snap.pumping = pumping;
    snapshot_changed = ui_service_set_machine_snapshot(&snap);
    if(snapshot_changed)
    {
      sl_ui_request_redraw();
    }

    for(tick_i = 0U; tick_i < (uint8_t)TICKS_INTERVAL; tick_i++)
    {
      sl_ui_tick_up();
    }
    sl_ui_run_once();

    page_name = sl_ui_current_page();
    if(s_task_ui_cfg.menu_active != 0)
    {
      *s_task_ui_cfg.menu_active = ((page_name != 0) &&
                                    (strcmp(page_name, "idle") != 0) &&
                                    (strcmp(page_name, "checking") != 0) &&
                                    (strcmp(page_name, "splash") != 0));
    }

    (void)xEventGroupSetBits(s_task_ui_cfg.event_group, s_task_ui_cfg.hb_bit);
    vTaskDelay(pdMS_TO_TICKS(TICKS_INTERVAL));
  }
}
