# SlateUI

`SlateUI` 是一个面向 MCU 的轻量图形界面框架（纯 C、静态内存、事件驱动）。

## 分层后的目录结构

```text
middleware/SlateUI/
|- AGENTS.md
|- README.md
|- README_zh-CN.md
|- core/
|  |- inc/
|  |  |- sl_display.h
|  |  |- sl_event.h
|  |  |- sl_key_repeat.h
|  |  |- sl_language.h
|  |  |- sl_page.h
|  |  |- sl_page_manager.h
|  |  `- sl_tween.h
|  `- src/
|     |- sl_display.c
|     |- sl_event.c
|     |- sl_key_repeat.c
|     |- sl_language.c
|     |- sl_page_manager.c
|     `- sl_tween.c
|- widgets/
|  |- inc/
|  |  |- sl_widget.h
|  |  |- sl_icon.h
|  |  |- sl_icon_item.h
|  |  |- sl_horizontal_menu.h
|  |  |- sl_label.h
|  |  |- sl_linear_layout.h
|  |  |- sl_list_view.h
|  |  `- sl_progress_bar.h
|  `- src/
|     |- sl_widget.c
|     |- sl_icon.c
|     |- sl_icon_item.c
|     |- sl_horizontal_menu.c
|     |- sl_label.c
|     |- sl_linear_layout.c
|     |- sl_list_view.c
|     `- sl_progress_bar.c
|- font/
|  |- sl_font.h
|  |- sl_font_ascii_16x16.h
|  |- sl_font_ascii_16x16.c
|  |- sl_font_chinese_16x16.h
|  `- sl_font_chinese_16x16.c
|- menu/
|  |- inc/
|  |  |- sl_menu_model.h
|  |  `- sl_menu_page.h
|  `- src/
|     |- sl_menu_model.c
|     `- sl_menu_page.c
`- port/
   |- sl_port.h
   `- sl_port.c
```

## 各层职责

- `core`：事件队列、页面栈、显示缓冲、语言表、UI 语义事件。
- `widgets`：控件基类与具体控件实现、控件 ID 与查找。
- `menu`：数据驱动的菜单模型与菜单页生命周期。MenuModel 描述菜单树，MenuPage 用 ListView 渲染。
- `font`：字库抽象与内建字模数据。
- `port`：移植层（屏幕发送、输入源、可选异步发送）。

## 控件 ID

每个控件现在持有稳定 `id` 字段（`sl_widget_id_t`，`uint16_t`）。
`0`（`SL_WIDGET_ID_NONE`）表示未命名，有效 ID 从 1 开始。

```c
void           sl_widget_set_id(sl_Widget *widget, sl_widget_id_t id);
sl_widget_id_t sl_widget_get_id(const sl_Widget *widget);
sl_Widget     *sl_widget_find_by_id(sl_Widget *root, sl_widget_id_t id);
const sl_Widget *sl_widget_find_by_id_const(const sl_Widget *root, sl_widget_id_t id);
```

同一页面内 ID 应唯一。`find_by_id` 深度优先遍历，返回首个匹配项。

## UI 语义事件

在原始输入事件（`sl_Event`）之上，SlateUI 新增了更高层的
UI 语义事件层（`sl_UiEvent`），用于前后台分离。

### 事件类型

| 枚举值 | 含义 |
|---|---|
| `SL_UI_EVT_FOCUS_CHANGED` | 焦点移至不同项 |
| `SL_UI_EVT_ENTER_ITEM` | 用户确认/进入当前项 |
| `SL_UI_EVT_BACK` | 用户请求返回 |
| `SL_UI_EVT_VALUE_CHANGED` | 可编辑值发生变化（尚未提交） |
| `SL_UI_EVT_VALUE_COMMIT` | 可编辑值已确认提交 |
| `SL_UI_EVT_ACTION_TRIGGERED` | 菜单动作被触发 |

### 事件载荷

```c
typedef struct {
    sl_ui_event_type_t type;
    sl_widget_id_t     widget_id;
    sl_action_id_t     action_id;
    int32_t            value;
    int32_t            value_prev;
    void              *context;
} sl_UiEvent;
```

### 接收 UI 事件

SlateUI 支持多订阅者，内部使用静态回调数组
（`SL_UI_EVENT_MAX_HANDLERS`，默认 4，可在编译选项中覆盖）。

```c
void presenter_handler(const sl_UiEvent *evt) { /* ... */ }
void logger_handler(const sl_UiEvent *evt)   { /* ... */ }

sl_ui_event_subscribe(presenter_handler);
sl_ui_event_subscribe(logger_handler);
```

兼容旧版单回调接口（清空所有订阅者后注册唯一回调）：

```c
sl_ui_event_set_handler(my_handler);
```

移除订阅者：

```c
sl_ui_event_unsubscribe(logger_handler);
```

`sl_ui_event_subscribe()` 在数组已满时返回 `false`。
重复订阅同一回调会被忽略（返回 `true`）。

### 发出 UI 事件的控件

- `sl_list_view`：光标移动时发出 `FOCUS_CHANGED`，按 ENTER 时发出 `ENTER_ITEM`。

## Action ID

`sl_action_id_t`（`uint16_t`）是菜单模型中使用的稳定动作标识符。
`SL_ACTION_NONE`（`0`）表示无动作。应用代码从 1 开始定义自己的枚举值。

## MenuModel（菜单模型）

MenuModel 是数据驱动的菜单描述层。一份静态数据数组即可描述完整设置页，无需手写页面逻辑。

### 菜单项类型

| 类型 | 显示 | 行为 |
|---|---|---|
| `SL_MENU_SUB_MENU` | "标题 >" | 按 ENTER 进入子菜单 |
| `SL_MENU_TOGGLE` | "标题 ON/OFF" | 按 ENTER 切换布尔值 |
| `SL_MENU_CHOICE` | "标题 选项名" | 按 ENTER 循环切换选项 |
| `SL_MENU_ACTION` | "标题" | 按 ENTER 调用 on_action 回调 |
| `SL_MENU_VALUE` | "标题 123" | 按 ENTER 进入行内编辑模式（UP/DOWN 调整） |

### 菜单项结构体

```c
typedef struct {
    const char       *text;          /* 显示文本（外部持有，不拷贝） */
    sl_MenuItemType   type;          /* 菜单项类型枚举 */
    int16_t           value;         /* 当前值（TOGGLE:0/1, CHOICE:选项索引, VALUE:数值） */
    int16_t           min;           /* 最小值（VALUE 类型使用） */
    int16_t           max;           /* 最大值（VALUE 类型使用） */
    const char      **choices;       /* 选项文本数组（CHOICE 类型，外部持有） */
    int16_t           choice_count;  /* 选项数量（CHOICE 类型使用） */
    sl_MenuActionCb   on_action;     /* 动作回调（ACTION 类型使用） */
    sl_MenuPageModel *sub;           /* 子菜单模型指针（SUB_MENU 类型使用） */
    sl_MenuValueGetter get_value;    /* 可选：外部值读取回调（NULL=直接读 item->value） */
    sl_MenuValueSetter set_value;    /* 可选：外部值写入回调（NULL=直接写 item->value） */
} sl_MenuItem;
```

### 值读写解耦（可选功能）

默认情况下 `value` 直接存储在 `sl_MenuItem` 中。若需要业务层持有数据真相
（如从硬件寄存器或 EEPROM 读取），可设置 `get_value` / `set_value` 回调：

```c
/* 示例：温度值由全局变量管理 */
int16_t temp_getter(const sl_MenuItem *item) { (void)item; return g_target_temp; }
void temp_setter(sl_MenuItem *item, int16_t val) {
    (void)item;
    if (val >= 0 && val <= 500) g_target_temp = val;
}

sl_MenuItem temp_item = {
    .text = "目标温度",
    .type = SL_MENU_VALUE,
    .min = 0, .max = 500,
    .get_value = temp_getter,
    .set_value = temp_setter,
};
```

当回调为 `NULL`（默认）时行为完全不变——完全向后兼容。

### 菜单页模型结构体

```c
typedef struct {
    const char       *title;      /* 页面标题文本（外部持有） */
    const sl_MenuItem *items;     /* 菜单项数组指针（外部持有） */
    int16_t           item_count; /* 菜单项数量 */
} sl_MenuPageModel;
```

### 模型辅助函数

```c
void        sl_menu_item_toggle(sl_MenuItem *item);              /* 切换 TOGGLE 项 */
void        sl_menu_item_next_choice(sl_MenuItem *item);         /* 循环切换 CHOICE 项 */
const char* sl_menu_item_get_choice_text(const sl_MenuItem *item);/* 获取 CHOICE 显示文本 */
const char* sl_menu_item_get_toggle_text(const sl_MenuItem *item);/* 获取 TOGGLE 显示文本 */
int16_t     sl_menu_item_get_value(const sl_MenuItem *item);     /* 读取值（经 getter 或直接读取） */
void        sl_menu_item_set_value(sl_MenuItem *item, int16_t v);/* 写入值（经 setter 或直接写入） */
```

## MenuPage（菜单页）

MenuPage 是 `sl_Page` 的实现，用 `sl_ListView` 控件渲染 `sl_MenuPageModel` 模型。
它处理光标导航、行内值编辑、子菜单进入，通过页面 presenter 回调处理交互。

### 使用方式

MenuPage 使用对象池分配（无需手动初始化）：

```c
/* 1. 定义菜单模型（静态数据） */
static const sl_MenuItem settings_items[] = {
    { .text = "亮度",   .type = SL_MENU_TOGGLE, .value = 1 },
    { .text = "模式",   .type = SL_MENU_CHOICE, .value = 0,
      .choices = (const char *[]){"自动", "手动"}, .choice_count = 2 },
    { .text = "目标温度", .type = SL_MENU_VALUE, .value = 200, .min = 0, .max = 500 },
    { .text = "保存",   .type = SL_MENU_ACTION, .on_action = on_save },
};

static const sl_MenuPageModel settings_model = {
    .title = "设置",
    .items = settings_items,
    .item_count = sizeof(settings_items) / sizeof(settings_items[0]),
};

/* 2. 分配并进入 */
sl_Page *menu_page = sl_menu_page_alloc(&settings_model);
if (menu_page) {
    sl_page_enter(menu_page);
}
```

池大小为 `SL_MENU_PAGE_POOL_SIZE`（默认 4，可在编译选项中覆盖）。
页面退出时自动释放槽位。

### 内部数据结构

```c
typedef struct {
    const sl_MenuPageModel *model;      /* 当前菜单模型指针（只读） */
    sl_ListView             list_view;  /* 列表视图控件实例 */
    sl_key_repeat_t         key_repeat; /* 按键重复生成器实例 */
    uint8_t                 editing;    /* 值编辑模式标志（1=编辑中，0=浏览） */
} sl_MenuPageData;
```

### 交互流程

| 交互 | 行为 |
|---|---|
| UP/DOWN | 移动光标（经 ListView，发出 FOCUS_CHANGED） |
| ENTER 子菜单 | 分配子页面并入栈 |
| ENTER TOGGLE | 切换 0↔1，刷新显示 |
| ENTER CHOICE | 循环切换到下一选项，刷新显示 |
| ENTER ACTION | 调用 `item->on_action(item)` |
| ENTER VALUE | 进入编辑模式（`editing=1`） |
| 编辑中 UP/DOWN | 在 `[min, max]` 范围内调整值，刷新 |
| 编辑中 ENTER/BACK | 退出编辑模式 |
| 浏览中 BACK | 返回 1 → 页面管理器弹出当前页 |

## Presenter（展示器）

每个 `sl_Page` 可选地携带一个 `presenter` 回调，同时接收 UI 语义事件和页面指针，
使 Presenter 能更新 UI 状态或调用 Service 函数。

```c
typedef void (*sl_PagePresenter)(const sl_UiEvent *evt, sl_Page *page);
```

### 用法

```c
void settings_presenter(const sl_UiEvent *evt, sl_Page *page) {
    if (evt->type == SL_UI_EVT_ENTER_ITEM) {
        sl_MenuPageData *data = (sl_MenuPageData *)page->data;
        const sl_MenuItem *item = &data->model->items[evt->value];
        /* 响应选中项：同步到 Service 层等 */
    }
}

page->presenter = settings_presenter;
```

MenuPage 在每次 UI 语义事件时自动调用 `page->presenter`。
自定义页面可在 `sl_ui_event_post()` 后手动调用 `page->presenter`。

### 双通道事件投递

UI 语义事件同时通过两个通道投递：

1. **全局订阅者**（`sl_ui_event_subscribe`）——用于日志、分析或跨页面协调
2. **页面 Presenter**（`page->presenter`）——用于页面特定的业务逻辑，
   接收页面指针可直接更新状态

## Service 边界

Service 函数位于应用代码中，不在 SlateUI 内。必须遵循以下规则：

- **命名**：`<领域>_service_<动词>(<参数>)` —— 如 `temp_service_set_target(int32_t)`
- **不含 UI 类型**：不得接受控件/页面指针，不得返回 UI 状态
- **UI 不直接访问全局变量**：UI 只能通过 Service 访问业务数据
- **返回值风格**：`0` = 成功，负数 = 错误

SlateUI 框架代码永远不直接调用 Service 函数。

## 页面间传参

页面进入时可接收参数，无需通过全局变量传递数据。

### `sl_page_enter_with`

```c
void sl_page_enter_with(sl_Page *new_page, void *arg);
```

在调用 `init` 之前设置 `new_page->arg = arg`，init 回调中通过
`self->arg` 读取参数并初始化页面私有状态。`arg` 是一次性参数，
页面通常在 init 中将其内容复制到自己的 `data` 中。

已有的 `sl_page_enter(page)` 等价于 `sl_page_enter_with(page, NULL)`。

### 示例

```c
typedef struct { int target_temp; int mode; } settings_params_t;

/* 调用方 */
settings_params_t params = { .target_temp = 200, .mode = 1 };
sl_page_enter_with(settings_page, &params);

/* 设置页 init */
static void settings_init(sl_Page *self) {
    settings_params_t *p = (settings_params_t *)self->arg;
    /* 将参数复制到页面私有状态 */
}

/* 返回值：通过 UI 语义事件 */
sl_UiEvent evt = { .type = SL_UI_EVT_VALUE_COMMIT, .value = new_temp, ... };
sl_ui_event_post(&evt);
```

## 工程接入说明

重构后请确认编译器包含路径：

- `middleware/SlateUI/core/inc`
- `middleware/SlateUI/widgets/inc`
- `middleware/SlateUI/menu/inc`
- `middleware/SlateUI/font`
- `middleware/SlateUI/port`

并把以下目录中的源码加入工程：

- `middleware/SlateUI/core/src`
- `middleware/SlateUI/widgets/src`
- `middleware/SlateUI/menu/src`
- `middleware/SlateUI/font`
- `middleware/SlateUI/port`

## 移植层接口摘要

来自 `port/sl_port.h`：

```c
void sl_port_init(void);
void sl_port_input_init(void);
void sl_port_poll_input(void);
void sl_hw_set_window(int x, int y, int w, int h);
void sl_hw_send_pixels(const uint8_t *data, int len);
```

可选开关：

```c
#define SL_USE_PINGPONG_BUF 0
#define SL_PORT_USE_ASYNC_TX 0
```

若要启用 DMA/异步显示发送，建议同时开启：

- `SL_USE_PINGPONG_BUF=1`
- `SL_PORT_USE_ASYNC_TX=1`

## 新增控件能力

- `sl_icon`：显示单色 1bpp 位图图标。
- `sl_icon_item`：显示图标和文字，并支持聚焦/选中状态。
- `sl_horizontal_menu`：管理横向图标菜单、光标移动和选中回调。

## 标签滚动

`sl_Label` 支持文本超宽时自动水平滚动。

```c
sl_Label title_label;
sl_label_init(&title_label, 0, 0, 128, 10, "很长的设置项名称", font, 1, SL_LABEL_ALIGN_LEFT);
sl_label_set_scroll(&title_label, SL_LABEL_SCROLL_AUTO);
```

在主循环或定时器回调中调用 `sl_label_tick(&label)` 推进滚动位置。
速度和暂停帧数可在编译时配置（`SL_LABEL_SCROLL_SPEED`、`SL_LABEL_SCROLL_PAUSE_FRAMES`）。

## 按键重复（长按加速）

`sl_key_repeat_t` 在持续按住方向键时自动生成重复事件，间隔逐渐加速。

```c
sl_key_repeat_t key_rep;
sl_key_repeat_init(&key_rep);

/* 事件处理中： */
sl_key_repeat_on_event(&key_rep, &event);

/* 定时器 tick 中（如每 10ms）： */
sl_key_repeat_tick(&key_rep, 10);
```

时序可配置：`SL_KEY_REPEAT_DELAY_MS`（初始延迟）、`SL_KEY_REPEAT_INTERVAL_MS`（首次重复间隔）、`SL_KEY_REPEAT_MIN_INTERVAL_MS`（最快间隔）、`SL_KEY_REPEAT_ACCEL_STEP_MS`（加速步进）。

## Tween（轻量动效）

`sl_tween_t` 提供纯整数插值，支持常用缓动曲线。无浮点，无动态分配。

```c
sl_tween_t tween;
sl_tween_start(&tween, 0, 100, 300, SL_TWEEN_EASE_OUT);

/* 定时器 tick 中： */
sl_tween_tick(&tween, delta_ms);
int32_t value = sl_tween_get_value(&tween);
```

可用曲线：`SL_TWEEN_LINEAR`、`SL_TWEEN_EASE_IN`、`SL_TWEEN_EASE_OUT`、`SL_TWEEN_EASE_IN_OUT`。

## 绘制辅助函数

```c
void sl_disp_draw_hline(int x, int y, int w, int color);
void sl_disp_draw_vline(int x, int y, int h, int color);
void sl_disp_draw_title_bar(const char *title, const void *font);
void sl_disp_draw_status_bar(int y, const char *text, const void *font);
```

`draw_title_bar` 在 y=0 处绘制 10px 高的反色标题栏。
`draw_status_bar` 在指定 y 位置绘制同样风格的底部状态栏。

## 光标动画

`sl_ListView` 集成了 `sl_tween_t`，光标移动时高亮条从旧位置平滑滑动到新位置，
使用 ease-out 缓动曲线。

动画时长可编译配置：`SL_LIST_CURSOR_ANIM_MS`（默认 120ms）。

在主循环或定时器回调中调用 `sl_list_view_tick(lv, delta_ms)` 推进动画。

## 页面过渡

页面进入/返回现在带有水平滑动过渡效果。旧页面向左滑出，新页面从右滑入，
使用 ease-out 缓动曲线。

过渡时长可编译配置：`SL_PAGE_TRANSITION_MS`（默认 150ms）。

在定时器回调中调用 `sl_page_manager_tick(delta_ms)` 驱动过渡动画。
过渡期间事件处理被锁定，防止误触。

## 滚动指示器

`sl_ListView` 可在右侧显示 2px 宽的滚动条，指示当前视口在整个列表中的位置。

```c
sl_list_view_set_scrollbar(&list_view, 1);
```

滚动条滑块的高度和位置根据可见行数和滚动偏移按比例计算。

## 输入说明

- `sl_horizontal_menu` 支持 `SL_EVT_KEY_LEFT` / `SL_EVT_KEY_RIGHT`。
- 也兼容 `SL_EVT_KEY_UP` / `SL_EVT_KEY_DOWN` 作为导航输入。
