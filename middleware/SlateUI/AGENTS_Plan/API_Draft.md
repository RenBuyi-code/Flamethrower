# SlateUI API 草案

## 1. 目标

本草案用于提前约束 SlateUI 后续演进中的核心接口，避免边写边漂移。

这不是最终头文件，只是接口设计方向。

重点覆盖：

- Widget ID
- UI 语义事件
- Action ID
- MenuModel
- Presenter / Service 边界

---

## 2. Widget ID 设计

### 2.1 类型建议

建议：

```c
typedef uint16_t sl_widget_id_t;
```

理由：

- MCU 足够轻量
- 比 `uint8_t` 更安全
- 比 `uint32_t` 更省

如后续确有需要，可在配置宏下切换到 `uint32_t`。

---

### 2.2 无效值

建议：

```c
#define SL_WIDGET_ID_NONE ((sl_widget_id_t)0u)
```

说明：

- `0` 表示未命名控件
- 页面内有效控件 ID 从 `1` 开始

---

### 2.3 sl_widget_t 增强建议

建议新增字段：

```c
typedef struct sl_widget {
    sl_widget_id_t id;
    ...
} sl_widget_t;
```

建议新增接口：

```c
void sl_widget_set_id(sl_widget_t *widget, sl_widget_id_t id);
sl_widget_id_t sl_widget_get_id(const sl_widget_t *widget);
sl_widget_t *sl_widget_find_by_id(sl_widget_t *root, sl_widget_id_t id);
const sl_widget_t *sl_widget_find_by_id_const(const sl_widget_t *root, sl_widget_id_t id);
```

约束：

- 同一页面内 ID 应唯一
- 框架默认不做复杂重复 ID 诊断
- 遇到重复 ID，返回第一个匹配项即可

---

## 3. UI 语义事件设计

### 3.1 为什么单独设计

当前原始输入事件更接近“物理按键/输入设备事件”，但前后台分离需要更高层语义。

因此建议保留两层：

- 原始输入事件
- UI 语义事件

---

### 3.2 事件类型

建议：

```c
typedef enum {
    SL_UI_EVT_NONE = 0,
    SL_UI_EVT_FOCUS_CHANGED,
    SL_UI_EVT_ENTER_ITEM,
    SL_UI_EVT_BACK,
    SL_UI_EVT_VALUE_CHANGED,
    SL_UI_EVT_VALUE_COMMIT,
    SL_UI_EVT_ACTION_TRIGGERED
} sl_ui_event_type_t;
```

---

### 3.3 事件载荷

建议：

```c
typedef struct {
    sl_ui_event_type_t type;
    sl_widget_id_t widget_id;
    uint16_t action_id;
    int32_t value;
    int32_t value_prev;
    void *context;
} sl_ui_event_t;
```

字段说明：

- `type`：事件类型
- `widget_id`：来源控件
- `action_id`：若由菜单动作触发，则给出动作编号
- `value`：当前值
- `value_prev`：变化前值
- `context`：为页面或控件预留扩展上下文

注意：

- `context` 只作为扩展位
- 默认使用中不应强依赖它

---

### 3.4 分发方式建议

可选方案 A：

- 页面直接接收 `sl_ui_event_t`

可选方案 B：

- 页面处理后再转给 Presenter

推荐：

- 页面负责本地 UI 状态变化
- Presenter 负责业务解释与联动

即：

- 局部显示问题在页面内消化
- 业务语义问题交给 Presenter

---

## 4. Action ID 设计

### 4.1 类型建议

```c
typedef uint16_t sl_action_id_t;
```

建议无效值：

```c
#define SL_ACTION_NONE ((sl_action_id_t)0u)
```

---

### 4.2 设计目的

让菜单项表达“动作语义”，而不是直接绑定函数。

例如：

```c
enum {
    SL_ACTION_OPEN_SETTINGS = 1,
    SL_ACTION_SET_TEMP,
    SL_ACTION_SAVE_CONFIG,
    SL_ACTION_FACTORY_RESET
};
```

这样做的好处：

- 菜单模型更稳定
- Presenter 更容易统一分发
- 不把业务函数指针直接塞进菜单树

---

## 5. MenuModel 草案

### 5.1 菜单项类型

建议：

```c
typedef enum {
    SL_MENU_ITEM_ACTION = 0,
    SL_MENU_ITEM_SUBMENU,
    SL_MENU_ITEM_INT_VALUE,
    SL_MENU_ITEM_TOGGLE,
    SL_MENU_ITEM_INFO
} sl_menu_item_type_t;
```

---

### 5.2 菜单项结构

建议：

```c
typedef struct sl_menu_page sl_menu_page_t;

typedef struct {
    uint16_t item_id;
    const char *title;
    sl_menu_item_type_t type;
    sl_action_id_t action_id;
    const sl_menu_page_t *submenu;
    int32_t min_value;
    int32_t max_value;
    int32_t step;
    uint8_t flags;
} sl_menu_item_t;
```

说明：

- `item_id`：菜单项稳定标识
- `title`：显示文本
- `type`：项类型
- `action_id`：动作语义
- `submenu`：若为子菜单则指向下一级页面
- `min/max/step`：供数值类项使用
- `flags`：禁用、隐藏、危险操作等标志位

---

### 5.3 菜单页结构

建议：

```c
struct sl_menu_page {
    uint16_t page_id;
    const char *title;
    const sl_menu_item_t *items;
    uint16_t item_count;
    void (*draw_extra)(void *ctx);
};
```

说明：

- `page_id`：页面稳定标识
- `title`：页面标题
- `items`：菜单项数组
- `item_count`：菜单项数量
- `draw_extra`：页面辅助绘制钩子

---

## 6. Page UI State 草案

建议页面显式持有 UI 状态：

```c
typedef struct {
    uint16_t focused_index;
    uint16_t first_visible_index;
    uint8_t editing;
    int32_t editing_value;
    uint8_t redraw_requested;
} sl_menu_page_state_t;
```

原则：

- 只存 UI 临时状态
- 不存业务真相状态

---

## 7. Presenter 接口草案

Presenter 不应侵入 SlateUI 核心，但应有清晰输入输出。

建议：

```c
typedef struct {
    void (*handle_ui_event)(const sl_ui_event_t *evt);
    void (*sync_view_state)(void);
} sl_presenter_t;
```

或应用侧自行定义更具体接口，例如：

```c
void app_settings_presenter_on_ui_event(const sl_ui_event_t *evt);
void app_settings_presenter_sync(void);
```

建议：

- 框架只提供事件结构
- Presenter 类型放应用侧定义

这样更轻，也更不耦合。

---

## 8. Service 接口建议

业务服务应保持纯业务语义。

示例：

```c
int temp_service_set_target(int32_t value);
int temp_service_get_target(int32_t *value);
int temp_service_get_current(int32_t *value);
```

要求：

- 不接受控件指针
- 不返回控件状态
- 不依赖页面结构

---

## 9. 事件流草案

建议未来按这条链路工作：

```c
input -> sl_event_post()
      -> page_manager
      -> widget/page internal handling
      -> sl_ui_event_t
      -> presenter
      -> service
      -> page ui state update
      -> sl_page_request_redraw()
```

这条链路的优点：

- 输入和业务彻底解耦
- 页面逻辑更可控
- 局部刷新更容易实现

---

## 10. 当前不建议引入的 API

现阶段不建议增加：

```c
sl_widget_find_by_name(...)
sl_bind_two_way(...)
sl_observe_property(...)
sl_runtime_reflect(...)
```

原因：

- 过重
- MCU 场景收益不高
- 容易把框架带偏

---

## 11. 推荐首批正式化的接口

如果要开始编码，建议优先正式化这些接口：

```c
typedef uint16_t sl_widget_id_t;
typedef uint16_t sl_action_id_t;

void sl_widget_set_id(sl_widget_t *widget, sl_widget_id_t id);
sl_widget_t *sl_widget_find_by_id(sl_widget_t *root, sl_widget_id_t id);

typedef enum {
    SL_UI_EVT_NONE = 0,
    SL_UI_EVT_FOCUS_CHANGED,
    SL_UI_EVT_ENTER_ITEM,
    SL_UI_EVT_BACK,
    SL_UI_EVT_VALUE_CHANGED,
    SL_UI_EVT_VALUE_COMMIT,
    SL_UI_EVT_ACTION_TRIGGERED
} sl_ui_event_type_t;
```

这几项一旦定下来，SlateUI 的前后台分离工作就真正可以开始了。
