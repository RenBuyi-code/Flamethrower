# SlateUI AI 编程指南

> 本文档面向 AI 编程助手，提供 SlateUI 框架的完整使用说明。
> 当你被要求使用 SlateUI 开发 MCU 应用界面时，请严格遵循本文档。

---

## 1. 框架概览

SlateUI 是一个面向 MCU 的轻量级 GUI 框架，纯 C 实现，零动态分配，事件驱动。

**核心设计原则：**
- 静态内存分配（无 malloc/free）
- 事件驱动（ISR 投递 → 主循环消费）
- 前后台分离（UI 语义事件 → Presenter → Service）
- 硬件抽象（所有平台代码集中在 port 层）

**目录结构：**
```
middleware/SlateUI/
├── core/       事件队列、页面栈、显存、语言、插值动画
├── widgets/    控件基类、Label、ListView、Icon、ProgressBar 等
├── menu/       数据驱动菜单模型与菜单页面
├── font/       字体抽象、8x16 ASCII 默认字库 + 16x16 中文 GB2312 字库
└── port/       硬件移植接口（用户必须实现）
```

---

## 2. 应用主循环模板

```c
#include "sl_port.h"
#include "sl_event.h"
#include "sl_page_manager.h"
#include "sl_key_repeat.h"

/* 定义根页面 */
static sl_Page root_page;

int main(void) {
    sl_port_init();              /* 1. 硬件初始化 */
    sl_port_input_init();        /* 2. 输入源初始化 */

    root_page = (sl_Page){       /* 3. 定义根页面 */
        .name      = "main",
        .init      = main_init,
        .draw      = main_draw,
        .proc      = main_proc,
        .exit      = NULL,
        .presenter = main_presenter,
        .data      = &main_data,
        .arg       = NULL
    };
    sl_page_manager_init(&root_page);  /* 4. 初始化页面管理器 */

    while (1) {
        sl_port_poll_input();          /* 5. 轮询输入（如使用轮询模式） */
        sl_page_manager_process();     /* 6. 事件分发 + 绘制 + 刷新 */
        sl_page_manager_tick(10);      /* 7. 推进过渡动画（delta_ms） */
    }
}
```

---

## 3. 页面生命周期

每个页面有 4 个回调 + 1 个 Presenter：

| 回调 | 调用时机 | 用途 |
|------|---------|------|
| `init(self)` | 页面入栈时 | 初始化控件、订阅事件 |
| `draw(self)` | 需要重绘时 | 通过 sl_disp_* 绘制界面 |
| `proc(self, event)` | 每帧事件分发 | 处理按键，返回 1 退出页面 |
| `exit(self)` | 页面出栈时 | 取消订阅、释放资源 |
| `presenter(evt, page)` | UI 语义事件产生时 | 业务逻辑处理 |

**页面间导航：**
```c
sl_page_enter(&new_page);                    /* 无参数进入 */
sl_page_enter_with(&new_page, &my_arg);      /* 带参数进入 */
sl_page_go_back();                           /* 返回上一页 */
```

---

## 4. 控件使用

### 4.1 控件树基础

所有控件继承 `sl_Widget`（必须为第一个成员），形成 first-child / next-sibling 树。

```c
sl_Widget root;
sl_widget_init(&root, 0, 0, 128, 64, NULL, NULL);

sl_Label title;
sl_label_init(&title, 0, 0, 128, 16, "Hello", &sl_default_font, 1, SL_LABEL_ALIGN_CENTER);
sl_widget_add_child(&root, &title.base);

sl_ListView list;
sl_list_view_init(&list, 0, 16, 128, 2, 18, &sl_default_font);
sl_widget_add_child(&root, &list.base);
```

### 4.1.1 字体选择

```c
/* 可用字体 */
&sl_default_font   /* 8x16 ASCII，默认，清晰易读（推荐用于标题、数值显示）*/
&sl_font_chinese   /* 16x16 中文（GB2312 常用子集，支持中英混合渲染）*/

/* 使用示例：英文标题用 ASCII 字体，中文标签用中文字体 */
sl_label_init(&title, 0, 0, 122, 16, "Settings", &sl_default_font, ...);    /* 8x16 ASCII */
sl_label_init(&menu_label, 0, 16, 122, 16, "设置", &sl_font_chinese, ...);     /* 16x16 中文 */

/* 自定义字体：实现 sl_FontDraw 回调 + 定义 sl_Font 实例即可挂接 */
```

### 4.2 Label（文本标签）

```c
sl_Label label;
sl_label_init(&label, x, y, w, h, "Text", font, color, SL_LABEL_ALIGN_LEFT);

/* 自动滚动（文本超出宽度时） */
sl_label_set_scroll(&label, SL_LABEL_SCROLL_AUTO);

/* 主循环中推进滚动 */
sl_label_tick(&label);
```

对齐方式：`SL_LABEL_ALIGN_LEFT` / `SL_LABEL_ALIGN_CENTER` / `SL_LABEL_ALIGN_RIGHT`

### 4.3 ListView（列表视图）

```c
sl_ListView lv;
sl_list_view_init(&lv, x, y, w, visible_count, item_height, font);

/* 设置数据 */
sl_ListItem items[] = {
    { .text = "Item 1" },
    { .text = "Item 2" },
    { .text = "Item 3" },
};
sl_list_view_set_items(&lv, items, 3);

/* 可选滚动条 */
sl_list_view_set_scrollbar(&lv, 1);

/* 主循环中推进光标动画 */
sl_list_view_tick(&lv, delta_ms);
```

**自动产生的 UI 语义事件：**
- 光标移动 → `SL_UI_EVT_FOCUS_CHANGED`（value = 新索引）
- 确认键 → `SL_UI_EVT_ENTER_ITEM`（value = 当前索引）

### 4.4 Icon（位图图标）

```c
sl_Icon icon;
sl_icon_init(&icon, x, y, w, h, &my_bitmap, color);
```

位图数据格式：
```c
const sl_IconBitmap my_bitmap = {
    .data   = bitmap_data,  /* 每行 ceil(width/8) 字节，MSB 在左 */
    .width  = 16,
    .height = 16
};
```

### 4.5 ProgressBar（进度条）

```c
sl_ProgressBar bar;
sl_progress_bar_init(&bar, x, y, w, h, min, max, fg_color, bg_color);
sl_progress_bar_set_value(&bar, 75);
```

### 4.6 LinearLayout（线性布局）

```c
sl_LinearLayout layout;
sl_linear_layout_init(&layout, x, y, w, h, SL_LAYOUT_VERTICAL, 2);
sl_widget_add_child(&layout.base, &child1.base);
sl_widget_add_child(&layout.base, &child2.base);
sl_linear_layout_apply(&layout);  /* 自动计算子控件坐标 */
```

### 4.7 HorizontalMenu（水平菜单）

```c
sl_HorizontalMenu menu;
sl_horizontal_menu_init(&menu, x, y, w, h, spacing);
sl_horizontal_menu_add_item(&menu, &item1);
sl_horizontal_menu_set_on_select(&menu, on_select_cb, NULL);
```

---

## 5. 数据驱动菜单（最常用模式）

### 5.1 定义菜单模型

```c
/* 菜单项类型：SL_MENU_SUB_MENU / SL_MENU_TOGGLE / SL_MENU_CHOICE / SL_MENU_ACTION / SL_MENU_VALUE */

static const char *lang_choices[] = { "English", "中文" };

static sl_MenuItem settings_items[] = {
    { .text = "亮度",   .type = SL_MENU_VALUE,  .value = 50, .min = 0, .max = 100 },
    { .text = "电源",   .type = SL_MENU_TOGGLE, .value = 1 },
    { .text = "语言",   .type = SL_MENU_CHOICE, .value = 0, .choices = lang_choices, .choice_count = 2 },
    { .text = "关于",   .type = SL_MENU_SUB_MENU, .sub = &about_model },
    { .text = "恢复出厂", .type = SL_MENU_ACTION, .on_action = on_factory_reset },
};

static const sl_MenuPageModel settings_model = {
    .title      = "设置",
    .items      = settings_items,
    .item_count = 5,
};
```

### 5.2 创建并进入菜单页面

```c
sl_Page *page = sl_menu_page_alloc(&settings_model);
if (page) {
    sl_page_enter(page);
}
```

### 5.3 菜单项交互行为

| 类型 | 确认键行为 | 显示格式 |
|------|-----------|---------|
| SUB_MENU | 进入子菜单页面 | "标题 >" |
| TOGGLE | 切换 0/1 | "标题 ON" / "标题 OFF" |
| CHOICE | 循环切换选项 | "标题 选项文本" |
| ACTION | 执行 on_action 回调 | "标题" |
| VALUE | 进入值编辑模式（上下键调整） | "标题 数值" |

### 5.4 值读写解耦（业务层持有数据真相）

默认情况下 `value` 直接存储在 `sl_MenuItem` 中。若需要业务层控制数据
（如从硬件寄存器/EEPROM 读取、校验范围），可设置回调：

```c
/* 业务层持有温度值 */
int16_t g_target_temp = 200;

int16_t temp_getter(const sl_MenuItem *item) { (void)item; return g_target_temp; }
void temp_setter(sl_MenuItem *item, int16_t val) {
    (void)item;
    if (val >= 0 && val <= 500) g_target_temp = val;
}

sl_MenuItem temp_item = {
    .text      = "目标温度",
    .type      = SL_MENU_VALUE,
    .min       = 0,
    .max       = 500,
    .get_value = temp_getter,
    .set_value = temp_setter,
};
```

**规则**：`get_value` / `set_value` 为 NULL 时行为不变（直接读写 `item->value`），完全向后兼容。

### 5.5 监听菜单事件

```c
void my_presenter(const sl_UiEvent *evt, sl_Page *page) {
    if (evt->type == SL_UI_EVT_ENTER_ITEM) {
        int index = evt->value;  /* 被选中项的索引 */
        /* 处理选中逻辑 */
    }
}

/* 在页面 init 中订阅 */
sl_ui_event_subscribe(self->presenter);

/* 在页面 exit 中取消订阅 */
sl_ui_event_unsubscribe(self->presenter);
```

---

## 6. 绘制 API 速查

```c
/* 像素与矩形 */
void sl_disp_draw_pixel(int x, int y, int color);
void sl_disp_fill_rect(int x, int y, int w, int h, int color);

/* 位图 */
void sl_disp_draw_bitmap_1bpp(int x, int y, int w, int h, const uint8_t *bitmap, uint8_t color);

/* 文本 */
uint16_t sl_disp_draw_string(uint16_t x, uint16_t y, const char *str, const void *font, uint8_t color);

/* 线段 */
void sl_disp_draw_hline(int x, int y, int w, int color);
void sl_disp_draw_vline(int x, int y, int h, int color);

/* 标题栏/状态栏（反色，10px 高） */
void sl_disp_draw_title_bar(const char *title, const void *font);
void sl_disp_draw_status_bar(int y, const char *text, const void *font);

/* 全局偏移（用于页面过渡动画） */
void sl_disp_set_offset(int dx, int dy);

/* 请求重绘 */
void sl_page_request_redraw(void);
```

**颜色约定：** `0` = 灭（暗），非 0 = 亮

---

## 7. 事件系统速查

### 原始输入事件（port 层 → 主循环）

```c
/* 在 ISR 或轮询中投递 */
sl_Event ev = { .type = SL_EVT_KEY_UP, .param = 0, .source = SL_EVT_SOURCE_RAW };
sl_event_post(&ev);
```

事件类型：`SL_EVT_KEY_UP/DOWN/LEFT/RIGHT/ENTER/BACK`、`SL_EVT_TIMER`、`SL_EVT_CUSTOM+`

### UI 语义事件（控件 → Presenter）

```c
/* 控件中投递 */
sl_UiEvent ui_evt = {
    .type       = SL_UI_EVT_VALUE_COMMIT,
    .widget_id  = my_widget_id,
    .action_id  = SL_ACTION_SET_TEMP,
    .value      = 200,
    .value_prev = 180,
    .context    = NULL
};
sl_ui_event_post(&ui_evt);

/* 订阅/取消订阅 */
sl_ui_event_subscribe(my_handler);
sl_ui_event_unsubscribe(my_handler);
```

语义事件类型：`FOCUS_CHANGED`、`ENTER_ITEM`、`BACK`、`VALUE_CHANGED`、`VALUE_COMMIT`、`ACTION_TRIGGERED`

---

## 8. 动画与定时

### 插值动画

```c
sl_tween_t tw;
sl_tween_start(&tw, from, to, duration_ms, SL_TWEEN_EASE_OUT);

/* 主循环中推进 */
sl_tween_tick(&tw, delta_ms);
int32_t val = sl_tween_get_value(&tw);
if (sl_tween_is_finished(&tw)) { /* 动画结束 */ }
```

缓动曲线：`SL_TWEEN_LINEAR` / `SL_TWEEN_EASE_IN` / `SL_TWEEN_EASE_OUT` / `SL_TWEEN_EASE_IN_OUT`

### 按键重复（长按加速）

```c
sl_key_repeat_t kr;
sl_key_repeat_init(&kr);

/* 事件处理中更新状态 */
sl_key_repeat_on_event(&kr, &event);

/* 定时器节拍中推进 */
sl_key_repeat_tick(&kr, delta_ms);
```

---

## 9. 多语言

```c
/* 设置语言 */
sl_lang_set(SL_LANG_CN);

/* 获取字符串 */
const char *text = SL_LANG(SL_STR_SETTINGS);

/* 切换后需手动刷新 */
sl_page_request_redraw();
```

---

## 10. 硬件移植（port 层）

用户必须实现以下函数（在 `port/sl_port.c` 中）：

| 函数 | 用途 |
|------|------|
| `sl_port_init()` | 系统时钟、GPIO、SPI/I2C、LCD 初始化 |
| `sl_hw_set_window(x,y,w,h)` | 设置 LCD 写入窗口 |
| `sl_hw_send_pixels(data,len)` | 同步发送像素数据 |
| `sl_port_input_init()` | 初始化按键输入（中断或轮询） |
| `sl_port_poll_input()` | 轮询按键状态并投递事件 |

**可选：** DMA 异步发送（`SL_PORT_USE_ASYNC_TX=1` + `SL_USE_PINGPONG_BUF=1`）

---

## 11. 编译配置

| 宏 | 默认值 | 说明 |
|----|-------|------|
| `SL_DISP_WIDTH` | 128 | 屏幕宽度 |
| `SL_DISP_HEIGHT` | 64 | 屏幕高度 |
| `SL_EVENT_QUEUE_SIZE` | 16 | 事件队列容量（2的幂） |
| `SL_UI_EVENT_MAX_HANDLERS` | 4 | UI 语义事件最大订阅数 |
| `SL_MAX_PAGE_DEPTH` | 8 | 页面栈最大深度 |
| `SL_PAGE_TRANSITION_MS` | 150 | 页面过渡动画时长 |
| `SL_LIST_CURSOR_ANIM_MS` | 120 | 光标动画时长 |
| `SL_MENU_PAGE_POOL_SIZE` | 4 | 菜单页面对象池大小 |
| `SL_KEY_REPEAT_DELAY_MS` | 500 | 长按首次延迟 |
| `SL_KEY_REPEAT_INTERVAL_MS` | 80 | 重复间隔 |
| `SL_KEY_REPEAT_MIN_INTERVAL_MS` | 20 | 最短重复间隔 |
| `SL_USE_PINGPONG_BUF` | 0 | 双缓冲开关 |
| `SL_PORT_USE_ASYNC_TX` | 0 | DMA 异步发送开关 |

---

## 12. 编码约束（必须遵守）

1. **禁止动态分配**：所有控件、页面、模型必须静态分配
2. **禁止跨层调用**：UI 代码不得直接调用 HAL，必须通过 port 层
3. **事件处理非阻塞**：proc/draw 回调中禁止阻塞等待
4. **API 前缀统一**：所有公开函数使用 `sl_` 前缀
5. **文本指针外部持有**：Label/MenuItem 的 text 指针不拷贝，调用方必须保证生命周期
6. **Service 边界**：业务函数命名 `<domain>_service_<verb>()`，UI 层不直接访问全局业务变量

---

## 13. 已知限制

1. **122×32 屏幕空间有限**：使用 8x16 ASCII 字体时仅能显示 ~2 行；使用 16x16 中文时仅能显示 1 行。需精心规划页面布局
2. **静态缓冲区**：`sl_menu_page.c` 中的 `list_items[16]` 和 `buf[32]` 为静态变量，非线程安全
3. **snprintf 依赖**：VALUE 类型显示使用 snprintf，部分 MCU 工具链可能较重
4. **菜单模型值解耦**：`sl_MenuItem` 现在支持可选的 `get_value`/`set_value` 回调（见 5.4 节），业务层可持有数据真相；不设置回调时行为不变
5. **中文子集有限**：`sl_font_chinese` 仅包含 64 个常用汉字，如需更多字符需扩展 `sl_chinese_glyphs[]` 表

---

## 14. 典型应用架构

```
┌─────────────────────────────────────────────┐
│                  main()                      │
│  sl_port_init() → sl_page_manager_init()     │
│  while(1) { process() + tick() + poll() }    │
└──────────────────┬──────────────────────────┘
                   │
    ┌──────────────┼──────────────┐
    ▼              ▼              ▼
┌────────┐  ┌──────────┐  ┌──────────┐
│  Page  │  │  Page    │  │  Page    │
│ (init/ │  │ (init/   │  │ (init/   │
│ draw/  │  │ draw/    │  │ draw/    │
│ proc/  │  │ proc/    │  │ proc/    │
│ exit)  │  │ exit)    │  │ exit)    │
└───┬────┘  └────┬─────┘  └────┬─────┘
    │            │              │
    ▼            ▼              ▼
┌──────────────────────────────────────┐
│        UI 语义事件 (sl_UiEvent)       │
│  FOCUS_CHANGED / ENTER_ITEM / ...    │
└──────────────┬───────────────────────┘
               │
    ┌──────────┼──────────┐
    ▼                     ▼
┌──────────┐       ┌──────────┐
│Presenter │       │ 全局订阅  │
│(页面级)   │       │(日志等)   │
└────┬─────┘       └──────────┘
     │
     ▼
┌──────────┐
│ Service  │  xxx_service_set(value)
│(业务层)   │  xxx_service_get() → int
└──────────┘
```
