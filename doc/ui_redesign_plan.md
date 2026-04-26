# UI 重规划设计

> 屏幕规格：**122×32 像素**，8×16 ASCII 字体
> 满屏可显示：**2 行文字**，每行约 **15 个英文字符**

---

## 一、当前 UI 问题分析

### 1.1 代码结构问题
- UI 代码全部内联在 `project/src/freertos_app.c`（1700+ 行），与 RTOS 任务创建、DMX 解析、执行器控制混杂
- 菜单数据（`sl_MenuItem` 数组）与回调函数全局静态定义，不可独立编译/测试
- 性能计时（DWT）与 UI 回调耦合

### 1.2 交互流程问题
- 当前按 MENU 直接进入菜单，没有明确的主界面实时状态显示
- ENTER 在 MenuPage 中的行为不直观（VALUE 类型进入编辑 vs 确认选中）
- 没有明确"保存"的反馈
- 菜单分组过多（4 组），在 2 行屏幕上深度深、操作繁琐

### 1.3 功能问题
- 无独立的实时主界面（Idle Screen）显示系统状态
- 保存参数后无显式确认提示

---

## 二、"压力设置" 英文翻译

| 中文 | 英文 |
|------|------|
| 压力设置 | **Pressure Set** |
| 语言设置 | **Language** |
| 中文 | **Chinese** |
| 英文 | **English** |

---

## 三、新菜单结构设计

### 3.1 整体结构（两层级）

```
┌─────────────────────────────────┐
│    主界面（Idle Screen）      │   ← 实时显示状态
│  READY DMX:ON  P:100%          │   ← 第1行：状态+DMX
│  Press MENU                    │   ← 第2行：提示/故障E1
├─────────────────────────────────┤
│          【MENU 键进入】         │
├─────────────────────────────────┤
│  主菜单（Main Menu, 3项滚动）   │
│  >1 DMX Set                    │   ← 第1行：焦点项
│   2 Pressure Set               │   ← 第2行：普通项
│   3 Language                   │   ← 向下滚动可见
├───────────┬─────────────────────┤
│           │ ENTER               │
│           ▼                     │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐
│  │ DMX Set      │  │ Pressure Set │  │ Language Set  │
│  │ Addr: 001    │  │ Ign: 050ms   │  │ >1 Chinese    │
│  │ Mode: 2CH    │  │ Lock: 050ms  │  │   2 English   │
│  │              │  │              │  │  ENTER→立即切换│
│  └──────────────┘  └──────────────┘  └───────────────┘
```


### 3.2 各页面详细规格

#### 3.2.1 主界面（Idle Screen）

| 项目 | 说明 |
|------|------|
| **用途** | 系统默认显示画面，开机后一直显示，实时反映系统状态 |
| **行1 (y=0~15)** | `状态  DMX:ON/OFF  P:xxx%` |
| | 示例：`RDY DMX:ON P:100%`（13 字符，宽度 OK） |
| | 状态缩写：`BOOT` / `TEST` / `RDY` / `FIRE` / `RELF` / `FLT` / `LCK` |
| | 无 DMX 时：`RDY DMX:-- P:100%` |
| **行2 (y=16~31)** | 显示故障码或提示 |
| | 无故障：`Press MENU`（提示用户） |
| | 有故障：`E1 E2 E3 E4 E5`（只显示激活的，如 `E1 E3`） |
| **按键** | MENU → 进入主菜单 |
| | UP / DOWN / ENTER → 无操作 |
| **实现** | 自定义 `sl_Page`，`draw()` 中 `sl_disp_fill_rect` + `sl_disp_draw_string` |

#### 3.2.2 主菜单（Main Menu）

| 项目 | 说明 |
|------|------|
| **用途** | 功能入口列表，3 项（DMX Set / Pressure Set / Language），2 行屏幕需滚动 |
| **行1 (y=0~15)** | `>1 DMX Set`（焦点项用 `>` 标记） |
| **行2 (y=16~31)** | ` 2 Pressure Set`（非焦点项缩进） |
|  | 滚动后：`>3 Language`（焦点在第 3 项时） |
| **按键** | UP → 光标上移（到第1项后停止）；DOWN → 光标下移（到第3项后停止） |
| | ENTER → 进入选中项的子设置页面 |
| | MENU → 返回主界面 |
| **实现** | 自定义 `sl_Page`，手写事件处理。维护 `focus_index`（0~2）和 `scroll_offset`（0 或 1） |

#### 3.2.3 DMX Set 页面

| 项目 | 说明 |
|------|------|
| **用途** | 设置 DMX 地址（001~512）和 DMX 模式（2CH/6CH） |
| **行1 (y=0~15)** | 浏览模式：`Addr: [001]`（焦点项用 `[ ]` 高亮） |
| | 编辑模式：`Addr:_001_`（编辑中项用 `_` 闪烁/下划线标记） |
| | **最多 15 字符**：`Addr: [001]` = 10 字符 ✓ |
| **行2 (y=16~31)** | 浏览模式：`Mode: [2CH]` |
| | 编辑模式：`Mode:_2CH_` |
| | **最多 15 字符**：`Mode: [2CH]` = 10 字符 ✓ |
| **页码指示** | 屏幕右侧可用像素指示当前页，如右上角显示 `1/2`（DMX 页面） |
| **交互流程** | |
| | **① 浏览模式**（进入页面时的默认状态）： |
| | - UP/DOWN → 焦点在 Addr 和 Mode 之间切换 |
| | - 焦点项用 `[值]` 显示，非焦点项用 `值` 显示 |
| | - ENTER → 进入**编辑模式**（焦点锁定在当前项） |
| | - MENU → 返回主菜单 |
| | |
| | **② 编辑模式**（ENTER 进入）： |
| | - Addr 编辑：UP = +1，DOWN = -1（范围 001~512，到边界停止） |
| | - Mode 编辑：UP = 切换 2CH→6CH，DOWN = 切换 6CH→2CH |
| | - ENTER → 保存值到 `g_app.params`，调用 `cfg_sanitize_params()`，回到浏览模式 |
| | - MENU → 取消编辑，丢弃未保存的修改，回到浏览模式 |
| **保存策略** | 单项保存（ENTER 确认时立即写 `g_app.params`），不自动写 EEPROM |
| **实现** | 自定义 `sl_Page`，用 `sl_disp_*` 直接绘制，手写事件处理 |

#### 3.2.4 Pressure Set 页面

| 项目 | 说明 |
|------|------|
| **用途** | 设置点火器延时（0~200ms）和锁油阀延时（0~200ms） |
| **行1 (y=0~15)** | 浏览模式：`Ign:  [050]ms`（焦点项 `[ ]` 高亮） |
| | 编辑模式：`Ign: _050_ms` |
| | 最大 15 字符：`Ign: [050]ms` = 12 字符 ✓ |
| **行2 (y=16~31)** | 浏览模式：`Lock: [050]ms` |
| | 编辑模式：`Lock:_050_ms` |
| | 最大 15 字符：`Lock: [050]ms` = 13 字符 ✓ |
| **交互流程** | |
| | **① 浏览模式**： |
| | - UP/DOWN → 焦点在 Ign 和 Lock 之间切换 |
| | - ENTER → 进入编辑模式 |
| | - MENU → 返回主菜单 |
| | |
| | **② 编辑模式**： |
| | - UP = +10ms，DOWN = -10ms（范围 0~200，到边界停止） |
| | - ENTER → 保存到 `g_app.params`，回到浏览模式 |
| | - MENU → 取消，回到浏览模式 |
| **实现** | 自定义 `sl_Page`，与 DMX Set Page 共用一套通用设置页逻辑（通过回调函数/参数区分） |

#### 3.2.5 Language Set 页面

| 项目 | 说明 |
|------|------|
| **用途** | 切换系统显示语言（中文 / English），切换后立即生效，Idle Screen 和主菜单文本同步更新 |
| **行1 (y=0~15)** | `>1 Chinese`（焦点项用 `>` 标记） |
| **行2 (y=16~31)** | `  2 English`（非焦点项缩进） |
| **交互流程** | |
| | UP → 光标上移；DOWN → 光标下移 |
| | ENTER → **立即切换语言**（调用 `sl_lang_set()`），该页面文本同步刷新 |
| | MENU → 返回主菜单 |
| **实现** | 自定义 `sl_Page`，无需编辑模式。ENTER 直接调用 `sl_lang_set(SL_LANG_EN/SL_LANG_CN)` + `sl_page_request_redraw()` |
| **依赖** | 依赖 `middleware/SlateUI/core/sl_language.h` 已有的 `SL_LANG_EN` / `SL_LANG_CN` 枚举和 `sl_lang_set()` / `sl_lang_get_current()` 接口。**无需修改 SlateUI 核心** |

---

## 四、按键行为完整定义

### 4.1 全局按键映射

| 按键 | 功能 | 说明 |
|------|------|------|
| **MENU** | 菜单开/关 | 主界面 → 进入主菜单；菜单内 → 退出到上一级 |
| **UP** | 导航上移 / 值+ | 浏览：向上移动焦点；编辑：增加值 |
| **DOWN** | 导航下移 / 值- | 浏览：向下移动焦点；编辑：减少值 |
| **ENTER** | 确认 / 保存 | 浏览：进入编辑模式；编辑：保存并退回浏览 |

### 4.2 页面 × 按键行为矩阵

| 页面 | MENU | UP | DOWN | ENTER |
|------|------|----|------|-------|
| **主界面 Idle** | → 主菜单 | 无操作 | 无操作 | 无操作 |
| **主菜单 Main** (3项) | → 主界面 | 光标上移(0→1→2) | 光标下移(2→1→0) | 进入选中子页 |
| **DMX Set** (浏览) | → 主菜单 | 焦点上移(Addr↔Mode) | 焦点下移(Addr↔Mode) | → 编辑模式 |
| **DMX Set** (编辑) | → 浏览模式(取消) | 值+1(Addr)/切换(Mode) | 值-1(Addr)/切换(Mode) | 保存→浏览模式 |
| **Pressure Set** (浏览) | → 主菜单 | 焦点上移(Ign↔Lock) | 焦点下移(Ign↔Lock) | → 编辑模式 |
| **Pressure Set** (编辑) | → 浏览模式(取消) | 值+10ms | 值-10ms | 保存→浏览模式 |
| **Language Set** | → 主菜单 | 光标上移(Chinese↔English) | 光标下移(Chinese↔English) | 立即切换语言 |

---

## 五、屏幕布局标准

### 5.1 基本参数
- 屏幕：122 × 32 像素，ST7920
- 字体：8×16 ASCII（每个字符 8×16 像素）
- 每行字符数：122 ÷ 8 = **15 字符**（留 2 像素右边距）
- 行数：32 ÷ 16 = **2 行**

### 5.2 行分配

```
y=0 ┌─────────────────────────────────────────┐
    │ ← 15 字符 →                           │ 行1 (y=0~15, page×2)
y=16├─────────────────────────────────────────┤
    │ ← 15 字符 →                           │ 行2 (y=16~31, page×2)
y=32└─────────────────────────────────────────┘
```

### 5.3 字体颜色约定

| 场景 | 绘制方式 |
|------|----------|
| 标题/状态文本 | `sl_disp_draw_string(..., 0)` — 白色字（黑底白字） |
| 焦点项（浏览模式） | `sl_disp_fill_rect` 反色背景 + `sl_disp_draw_string(..., 1)` 黑字 |
| 编辑中项 | 反色背景 + 值部分下划线或闪烁（简单实现：保存后直接回浏览模式） |
| 普通文本（非焦点） | `sl_disp_draw_string(..., 0)` 正常白色 |

---

## 六、实现方案

### 方案选择：新建独立页面文件（推荐）

| 步骤 | 内容 | 工作量 |
|------|------|--------|
| 1 | 新建 `app/ui_pages/ui_idle_page.c/.h` — 主界面 | 中 |
| 2 | 新建 `app/ui_pages/ui_main_menu.c/.h` — 主菜单 | 中 |
| 3 | 新建 `app/ui_pages/ui_setting_page.c/.h` — 通用设置页 | 中 |
| 4 | 新建 `app/ui_pages/ui_language_page.c/.h` — 语言切换页 | 小 |
| 5 | 在 `freertos_app.c` 中集成新页面 | 中 |
| 6 | 删除旧 UI inline 代码（菜单数据+回调） | 小 |
| 7 | 验证编译和功能 | 中 |

---

## 七、实现任务拆解

### P0 — 新建目录和文件

- [ ] **T-UI-01**: 新建 `app/ui_pages/ui_idle_page.h / ui_idle_page.c`
  - 输入：`app_core_t *g_app` 指针（通过外部传入或全局）
  - 输出：`sl_Page` 实例 + `draw()` 绘制函数
  - 验收：初始化后显示 READY 状态和 DMX/Pressure 信息

- [ ] **T-UI-02**: 新建 `app/ui_pages/ui_main_menu.h / ui_main_menu.c`
  - 输入：菜单项文本数组、选中回调函数指针
  - 输出：`sl_Page` 实例，支持 2 项列表选择
  - 验收：UP/DOWN 切换焦点，ENTER 触发回调，MENU 返回

- [ ] **T-UI-03**: 新建 `app/ui_pages/ui_setting_page.h / ui_setting_page.c`
  - 功能：通用两参数设置页面，支持浏览/编辑模式
  - 接口：通过结构体配置参数名、值读写回调、步进、范围
  - 验收：DMX 和 Pressure 页面可共用此模板

- [ ] **T-UI-04**: 新建 `app/ui_pages/ui_language_page.h / ui_language_page.c`
  - 功能：中/英文切换页面，选择后立即生效
  - 接口：调用 `sl_lang_set()` + `sl_page_request_redraw()`
  - 验收：选择 Chinese 后 Idle 和 MainMenu 文本变为中文，选择 English 恢复英文

### P1 — 集成到 freertos_app.c

- [ ] **T-UI-05**: 重构 `ui_task` 和 `ui_setup_once`
  - 根页面改为 Idle Screen Page
  - MENU 事件改为 Idle←→MainMenu 切换
  - 删除旧 `sl_MenuItem` 静态数据（s_dmx_menu_items, s_timing_menu_items 等约 200 行）

- [ ] **T-UI-06**: 删除旧 UI 按键回调（`ui_btn_click_cb`、`ui_btn_repeat_cb`、`ui_ui_event_log_cb`）
  - 替换为新的按键事件处理（直接路由到当前活跃 Page 的 proc）

- [ ] **T-UI-07**: 清理与 UI 无直接关系的性能计时变量（`g_ui_perf_last_key_us`、`g_ui_perf_last_evt_us`）
  - 如果 `ui_btn_click_cb` 中使用了这些变量，需一并清理

### P2 — 验证

- [ ] **T-UI-08**: 编译检查，0 Error 0 Warning
- [ ] **T-UI-09**: 功能验证 — 主界面显示系统状态
- [ ] **T-UI-10**: 功能验证 — MENU 进入/退出主菜单
- [ ] **T-UI-11**: 功能验证 — DMX Set 页面修改地址和模式，观察 g_app.params 变化
- [ ] **T-UI-12**: 功能验证 — Pressure Set 页面修改延时值，观察 g_app.params 变化
- [ ] **T-UI-13**: 功能验证 — 编辑模式下 MENU 取消不保存，值恢复修改前
- [ ] **T-UI-14**: 功能验证 — Language Set 页面切换中/英文，所有页面文本同步更新

---

## 八、设计决策记录

### 8.1 为什么自定义 Page 而非 MenuPage
- MenuPage 的 VALUE 类型编辑模式在 2 行屏幕上操作路径长
- 自定义 Page 可以实现"浏览按 ENTER 编辑、UP/DOWN 改值、ENTER 保存、MENU 取消"的直观流程
- 对 122×32 小屏幕的布局控制更精确

### 8.2 主菜单用自定义 Page 而非 MenuPage
- 菜单只有 2 项，自定义 Page 实现简单（一个焦点索引即可）
- 避免引入 MenuPage 对象池和 ListView 等重量级依赖
- 减少代码依赖，新文件不依赖 SlateUI 内部细节（仅依赖 `sl_page.h`、`sl_event.h`、`sl_display.h`）

### 8.3 参数保存策略
- **运行时保存**：ENTER 确认单项时立即 `cfg_sanitize_params()` + 写 `g_app.params`
- **EEPROM 落盘**：暂不自动保存。维持现有 Save Params 动作，或后续增加"退出设置时自动保存全部参数"
- 这样避免频繁写 EEPROM（磨损），同时确保运行时参数一致性

### 8.4 Idle Screen 更新策略
- 固定在 `ui_task` 的 tick 周期（每 5ms）刷新
- 状态变化（如 READY→FIRING）或压力变化 > 5% 才重绘
- 减少不必要的屏幕刷新

---

## 九、涉及文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `app/ui_pages/ui_idle_page.c` | **新建** | 主界面 Idle Screen 实现 |
| `app/ui_pages/ui_idle_page.h` | **新建** | 主界面头文件 |
| `app/ui_pages/ui_main_menu.c` | **新建** | 主菜单实现（3项可滚动） |
| `app/ui_pages/ui_main_menu.h` | **新建** | 主菜单头文件 |
| `app/ui_pages/ui_setting_page.c` | **新建** | 通用两参数设置页面实现（DMX/Pressure 共用） |
| `app/ui_pages/ui_setting_page.h` | **新建** | 设置页面头文件 |
| `app/ui_pages/ui_language_page.c` | **新建** | 语言切换页面实现 |
| `app/ui_pages/ui_language_page.h` | **新建** | 语言切换页面头文件 |
| `project/src/freertos_app.c` | **修改** | 删除旧 UI inline 代码，集成新页面；主菜单改为3项 |
| `doc/工程TODO_AI执行版.md` | **更新** | 追加 UI 重构条目 |
