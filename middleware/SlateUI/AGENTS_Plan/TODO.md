# SlateUI 落地 TODO

## 1. 使用规则

本清单用于把 `AGENTS_Plan/README.md` 中的方向，拆成可以逐项执行的落地任务。

执行原则：

- 优先做基础边界，后做体验增强
- 优先做低耦合公共能力，后做具体页面效果
- 每次提交保持范围小、可验证、可回退
- 不在同一轮同时大改核心结构和视觉表现

***

## 2. P0：分层基础能力 ✅ 已完成

### 2.1 Widget ID ✅

- [x] 在 `sl_Widget` 中增加 `id` 字段（`sl_widget_id_t`，`uint16_t`）
- [x] 定义 `SL_WIDGET_ID_NONE` 常量
- [x] `sl_widget_init` 初始化 `id = SL_WIDGET_ID_NONE`

### 2.2 Find By ID ✅

- [x] `sl_widget_find_by_id(root, id)` — 深度优先遍历
- [x] `sl_widget_find_by_id_const` — const 版本
- [x] 约定同一页面内 ID 唯一，找到第一个匹配项立即返回

### 2.3 UI 语义事件 ✅

- [x] 定义 `sl_ui_event_type_t` 枚举（6 种语义事件）
- [x] 定义 `sl_UiEvent` 结构体（type + widget_id + action_id + value + value_prev + context）
- [x] 定义 `sl_action_id_t` 类型和 `SL_ACTION_NONE` 常量
- [x] 实现 `sl_ui_event_post` / `sl_ui_event_subscribe` / `sl_ui_event_unsubscribe` / `sl_ui_event_set_handler`
- [x] 多订阅者静态数组（`SL_UI_EVENT_MAX_HANDLERS`，默认 4）

### 2.4 可编辑控件变更事件 ✅

- [x] `sl_list_view`：光标移动发出 `FOCUS_CHANGED`，ENTER 发出 `ENTER_ITEM`
- [x] 删除 `sl_int_edit`（不通用，行内编辑由 MenuPage 替代）

***

## 3. P1：菜单语义层 ✅ 已完成

### 3.1 MenuModel ✅

- [x] 定义 `sl_MenuItem` 结构（5 种类型：SUB_MENU / TOGGLE / CHOICE / ACTION / VALUE）
- [x] 定义 `sl_MenuPageModel` 结构（title + items + item_count）
- [x] 实现 `sl_menu_item_toggle` / `sl_menu_item_next_choice` / `sl_menu_item_get_choice_text` / `sl_menu_item_get_toggle_text`

### 3.2 Action ID ✅

- [x] `sl_action_id_t` 类型已定义（P0 阶段）
- [x] MenuModel 中每个菜单项携带 `action_id`
- [x] 菜单项只表达"做什么"，不表达"怎么做"

### 3.3 MenuPage 与 View 解耦 ✅

- [x] `sl_MenuPageData` 持有 model + list_view + key_repeat + editing
- [x] MenuPage 只持有菜单语义，ListView 负责列表呈现
- [x] `sl_menu_page_alloc` 对象池分配，退出时自动释放
- [x] 静态池分配，退出时自动释放

***

## 4. P2：前后台桥梁 ✅ 已完成

### 4.1 Presenter / Controller 层 ✅

- [x] `sl_Page` 增加 `presenter` 回调字段（`sl_PagePresenter`）
- [x] MenuPage 通过双通道投递（全局订阅 + 页面 Presenter）
- [x] Presenter 输入：UI 语义事件 + 页面指针
- [x] Presenter 输出：更新 UI state / 调用 service / 请求重绘

### 4.2 Service 接口边界 ✅

- [x] AGENTS.md 中明确 Service 命名规则（`<domain>_service_<verb>`）
- [x] 明确返回值风格（0 = 成功，负数 = 错误）
- [x] 明确 UI 不可直接访问全局业务变量
- [x] 明确 SlateUI 框架代码永远不直接调用 Service 函数

### 4.3 Page UI State ✅

- [x] `sl_MenuPageData` 集中管理（model + list_view + key_repeat + editing）
- [x] MenuPage 事件处理中同步 ListView 状态到 UI State
- [x] 区分 UI state 和业务状态

***

## 5. P3：体验增强 ✅ 已完成

### 5.1 长文本滚动 ✅

- [x] `sl_Label` 增加 `scroll` / `scroll_offset` / `scroll_pause` 字段
- [x] `sl_label_set_scroll` 设置滚动模式
- [x] `sl_label_tick` 推进滚动位置
- [x] 超宽文本自动滚动，循环播放，首尾暂停
- [x] 速度和暂停帧数可编译配置

### 5.2 长按加速 ✅

- [x] `sl_key_repeat_t` 模块
- [x] `sl_key_repeat_on_event` 激活/停止重复
- [x] `sl_key_repeat_tick` 定时器驱动，加速曲线
- [x] 初始延迟 → 首次间隔 → 逐步加速 → 最快间隔
- [x] 时序参数可编译配置

### 5.3 页面辅助绘制 ✅

- [x] `sl_disp_draw_hline` / `sl_disp_draw_vline` 绘制线段
- [x] `sl_disp_draw_title_bar` 绘制反色标题栏
- [x] `sl_disp_draw_status_bar` 绘制底部状态栏

### 5.4 轻量动效 ✅

- [x] `sl_tween_t` 纯整数插值器
- [x] 4 种缓动曲线（LINEAR / EASE_IN / EASE_OUT / EASE_IN_OUT）
- [x] `sl_tween_start` / `sl_tween_tick` / `sl_tween_get_value`
- [x] 无浮点，无动态分配，256 定点数

***

## 6. P4：视觉动效集成 ✅ 已完成

### 6.1 焦点动画 ✅

- [x] `sl_ListView` 增加 `cursor_tween`（sl_tween_t）字段
- [x] 光标移动时启动 Tween，从旧位置滑到新位置
- [x] `list_draw` 用 `cursor_tween.current` 计算高亮条 Y 坐标
- [x] `sl_list_view_tick` 推进 Tween，活跃时请求重绘
- [x] 动画时长可编译配置（`SL_LIST_CURSOR_ANIM_MS`，默认 120ms）

### 6.2 页面过渡 ✅

- [x] `sl_page_manager` 增加 `trans_tween`（sl_tween_t）字段
- [x] 进入新页面时：旧页面向左滑出，新页面从右滑入
- [x] 返回上一页时：当前页面向右滑出，上一页从左滑入
- [x] 过渡期间锁定事件处理
- [x] `sl_page_manager_tick` 推进过渡动画
- [x] 过渡时长可编译配置（`SL_PAGE_TRANSITION_MS`，默认 150ms）

### 6.3 滚动指示器 ✅

- [x] `sl_ListView` 增加 `show_scrollbar` 字段
- [x] `sl_list_view_set_scrollbar` 开关
- [x] `list_draw` 在右侧 2px 宽区域绘制滚动条
- [x] 滚动条高度 = 可见行数 / 总行数 × 列表高度
- [x] 滚动条位置 = scroll_offset / (总行数 - 可见行数) × 可滑动区域

***

## 7. 额外完成项

- [x] 页面间传参：`sl_page_enter_with(page, arg)` + `sl_Page.arg` 字段
- [x] 删除 `sl_int_edit`（不通用，由 MenuPage 行内编辑替代）
- [x] `sl_MenuItem` 支持 TOGGLE/CHOICE/ACTION 回调
- [x] MenuPage 编辑模式边界检查
- [x] Pool 默认大小 4（MCU RAM 友好）
- [x] 工业级注释（UCOSII + STM32 HAL 风格，28 个文件全覆盖）
- [x] AI 使用说明文档（AI_USAGE_GUIDE.md）
- [x] 字体系统升级：默认字体 8x16 ASCII（95字符）+ 16x16 中文 GB2312（64常用字），移除 6x8 字体
- [x] 适配 8x16 字体：标题栏/状态栏高度改为 16px，菜单项高度改为 18px

***

## 8. 每轮开发检查项

- [x] 是否引入层级越界 → 无
- [x] 是否破坏现有公开 API → 无，全部向后兼容
- [x] 是否新增了隐藏耦合 → 无
- [x] 是否把业务状态塞进控件 → 无，MenuModel value 是 UI 层临时状态
- [x] 是否需要同步 README → 已同步
- [x] 是否保持 MCU 友好 → 是，无动态分配，无浮点

***

## 9. 残余限制

- ~~`sl_MenuItem.value` 直接存储当前值，业务真相与 UI 显示值耦合~~ → **已解决**：新增 `get_value`/`set_value` 可选回调（[sl_menu_model.h](menu/inc/sl_menu_model.h)），NULL 时行为不变，完全向后兼容
- `snprintf` 在部分 MCU 工具链上可能较重，可考虑用轻量 `itoa` 替代
- 未经过编译验证（无可用构建环境）
- ~~README.md 中 MenuModel API 描述与实际代码有差异~~ → **已解决**：README.md / README_zh-CN.md 已对齐代码实际 API
- **122×32 屏幕空间有限**：使用 8x16 ASCII 时仅能显示 ~2 行，使用 16x16 中文时仅能显示 1 行。需精心规划页面布局
