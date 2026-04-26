# SlateUI 面向对象重构 TODO

## 0. 目标

将 SlateUI 从当前的"页面 + freertos_app 胶水"模式，重构为**类 Android Activity 架构**：

- 每个页面（Layout）自包含业务逻辑，独立可跳转
- 引入页面注册机制，由 UI 管理器统一调度
- UI 拥有独立时基（`sl_ui_tick_up()`），不再依赖外部 tick 驱动
- 消除 `freertos_app.c` 中的 consume 轮询胶水代码
- 页面间通过导航请求（Intent）自由跳转，而非外部 if-else 分发

## 1. 当前问题分析

### 1.1 前后台未分离

```
当前架构：
  freertos_app.c (ui_task)
    ├── button_ticks() -> sl_event_post()
    ├── ui_idle_page_update(...)        ← 业务直接推送数据到页面
    ├── ui_checking_page_update(...)
    ├── sl_page_manager_process()
    ├── sl_page_manager_tick(20)
    ├── ui_xxx_page_consume_xxx()       ← 轮询页面状态标志
    │     └── if(consume) sl_page_enter(...)  ← 胶水代码决定跳转
    ├── ui_language_page_consume_selection() -> app_params_commit()
    └── ui_safety_page_consume_tilt_changed() -> ui_setting_on_tilt_save()
```

问题：
- 页面流转逻辑散落在 `freertos_app.c`，而非页面自身
- 每新增一个页面，都要在 `freertos_app.c` 加 consume + if-else
- 业务回调（`on_save`、`consume`）让 `freertos_app.c` 越来越臃肿
- 框架已设计 `sl_UiEvent` + `presenter` 机制但完全未使用

### 1.2 页面没有自主导航能力

当前页面只能通过 `proc()` 返回 1 来"请求退出"，无法主动跳转到指定页面。
跳转目标完全由外部 `freertos_app.c` 决定，页面自身不知道下一个页面是谁。

### 1.3 UI 没有独立时基

`sl_page_manager_tick()` 由 `ui_task` 手动调用，传入固定 `TICKS_INTERVAL`。
UI 无法自主感知时间，定时器事件（如 splash 超时）依赖外部 tick。

### 1.4 页面注册缺失

页面对象分散在各 `.c` 文件中，通过 `ui_xxx_page_get()` 工厂函数获取。
没有统一的注册表，页面间无法通过名字查找和跳转。

## 2. 目标架构

```
重构后架构：
  1ms ISR
    └── sl_ui_tick_up()                 ← UI 独立时基

  ui_task (FreeRTOS)
    ├── button_ticks() -> sl_event_post()
    ├── sl_ui_run()                     ← 一个函数驱动全部 UI
    │     ├── sl_ui_tick_dispatch()     ← 时基分发（定时器、动画、超时）
    │     ├── sl_ui_event_dispatch()    ← 事件分发到当前页面
    │     └── sl_ui_render()            ← 绘制 + flush
    └── (无 consume 胶水代码)

  页面 (ui_xxx_page.c)
    ├── init() / draw() / proc() / exit()
    ├── on_create() / on_destroy()      ← 生命周期扩展
    ├── sl_ui_navigate("target_page")   ← 页面自主导航
    ├── sl_ui_go_back()                 ← 页面自主返回
    ├── sl_ui_set_result(result)        ← 向调用者返回结果
    └── 业务逻辑自包含                  ← 不再依赖外部 consume
```

### 2.1 核心概念对照

| Android 概念 | SlateUI 对应 | 说明 |
|---|---|---|
| Activity | `sl_Page` | 页面生命周期单元 |
| Intent | `sl_NavIntent` | 导航请求（目标页面名 + 参数） |
| ActivityManager | `sl_ui_manager` | 页面注册表 + 栈管理 + 调度 |
| `startActivity()` | `sl_ui_navigate()` | 页面发起跳转 |
| `finish()` | `sl_ui_go_back()` | 页面请求退出 |
| `setResult()` | `sl_ui_set_result()` | 向前页面返回结果 |
| `onCreate/onDestroy` | `on_create/on_destroy` | 生命周期扩展回调 |
| `onResume/onPause` | `on_resume/on_pause` | 前后台切换回调 |
| `Handler.postDelayed()` | `sl_ui_post_delayed()` | 页面定时器 |
| `LayoutInflater` | `draw()` | 页面布局绘制 |

## 3. 实施计划

### Phase 1: UI 独立时基

- [ ] T1-1 新增 `sl_ui_tick_up()` 函数
  - 声明在 `sl_page_manager.h` 或新建 `sl_ui_clock.h`
  - 实现：累加全局毫秒计数器 `sl_ui_clock_ms`
  - 放入 1ms SysTick 中断中调用
  - 替代当前 `sl_page_manager_tick(delta_ms)` 的外部驱动方式

- [ ] T1-2 新增 `sl_ui_get_tick()` 获取当前 UI 时钟
  - 页面内部可用此函数实现定时器、超时、动画

- [ ] T1-3 重构 `sl_page_manager_tick()` 为内部自动驱动
  - `sl_page_manager_process()` 内部自动计算 delta_ms
  - 不再需要外部传入 delta_ms

### Phase 2: 页面注册机制

- [ ] T2-1 定义页面注册表
  ```c
  // sl_page_registry.h
  #define SL_MAX_PAGES  16

  typedef struct {
      const char *name;           // 页面名称，如 "idle"、"main_menu"
      sl_Page* (*factory)(void);  // 页面工厂函数
  } sl_PageEntry;

  void sl_ui_register(const char *name, sl_Page* (*factory)(void));
  sl_Page* sl_ui_resolve(const char *name);
  ```

- [ ] T2-2 各页面在初始化时自注册
  ```c
  // ui_idle_page.c
  void ui_idle_page_register(void) {
      sl_ui_register("idle", ui_idle_page_get);
  }
  ```

- [ ] T2-3 定义启动页面（根页面）
  ```c
  void sl_ui_set_root(const char *name);
  ```

### Phase 3: 导航系统

- [ ] T3-1 定义导航意图结构体
  ```c
  typedef struct {
      const char *target;     // 目标页面名
      void       *arg;        // 传递参数
      uint8_t     flags;      // 导航标志（清除栈、动画等）
  } sl_NavIntent;

  #define SL_NAV_FLAG_CLEAR_STACK  0x01  // 清除页面栈
  #define SL_NAV_FLAG_NO_ANIM      0x02  // 无动画
  ```

- [ ] T3-2 实现页面自主导航 API
  ```c
  // 页面内部调用
  void sl_ui_navigate(const char *target);              // 跳转到指定页面
  void sl_ui_navigate_with(const char *target, void *arg); // 带参数跳转
  void sl_ui_go_back(void);                              // 返回上一页
  void sl_ui_set_result(int code, void *data);           // 设置返回结果
  ```

- [ ] T3-3 页面接收返回结果
  ```c
  struct sl_Page {
      // ... 现有字段 ...
      void (*on_result)(sl_Page *self, int code, void *data);  // 接收子页面返回
  };
  ```

### Phase 4: 页面生命周期扩展

- [ ] T4-1 扩展 `sl_Page` 结构体
  ```c
  struct sl_Page {
      const char       *name;
      sl_PageInit       init;         // 入栈时调用
      sl_PageDraw       draw;
      sl_PageProc       proc;
      sl_PageExit       exit;         // 出栈时调用
      sl_PagePresenter  presenter;

      // 新增生命周期回调
      void (*on_resume)(sl_Page *self);   // 页面成为前台时调用
      void (*on_pause)(sl_Page *self);    // 页面进入后台时调用
      void (*on_result)(sl_Page *self, int code, void *data);  // 接收子页面结果
      void (*on_tick)(sl_Page *self, uint32_t elapsed_ms);     // 时基回调

      void             *data;
      void             *arg;
  };
  ```

- [ ] T4-2 实现 `on_resume` / `on_pause` 调用时机
  - `sl_page_enter()` 时：旧页面 `on_pause()`，新页面 `init()` + `on_resume()`
  - `sl_page_go_back()` 时：当前页面 `exit()`，恢复页面 `on_resume()`

- [ ] T4-3 实现页面定时器
  ```c
  // 页面可注册延迟回调
  void sl_ui_post_delayed(sl_Page *self, uint32_t delay_ms, void (*callback)(sl_Page *));
  ```
  - 内部使用 `sl_ui_clock_ms` 驱动
  - 页面退出时自动取消未触发的定时器

### Phase 5: 业务逻辑内聚到页面

- [ ] T5-1 重构 `ui_idle_page.c`
  - 当前：外部调用 `ui_idle_page_update()` 推送数据
  - 目标：页面在 `on_tick()` 中主动拉取业务数据（通过 service 接口）
  - 导航：MENU 键 -> `sl_ui_navigate("main_menu")`

- [ ] T5-2 重构 `ui_main_menu.c`
  - 当前：外部 `consume_selected()` + if-else 分发跳转
  - 目标：ENTER 键 -> `sl_ui_navigate(s_item_targets[cursor])`
  - 菜单项增加目标页面名映射：
    ```c
    static const char *s_item_targets[4] = {
        "dmx_set", "pressure_set", "safety", "language"
    };
    ```

- [ ] T5-3 重构 `ui_language_page.c`
  - 当前：外部 `consume_selection()` -> `app_params_commit()`
  - 目标：ENTER 键 -> 内部调用 `lang_service_save()` + `sl_ui_go_back()`
  - 返回结果：`sl_ui_set_result(lang_id, NULL)`

- [ ] T5-4 重构 `ui_safety_page.c`
  - 当前：外部 `consume_tilt_changed()` -> `ui_setting_on_tilt_enable_save()`
  - 目标：切换时内部调用 `safety_service_set_tilt()` + 请求重绘
  - 返回结果：`sl_ui_set_result(tilt_value, NULL)`

- [ ] T5-5 重构 `ui_setting_page.c`
  - 当前：`on_save` 回调由外部注册
  - 目标：页面内部直接调用 `params_service_save_xxx()`

- [ ] T5-6 重构 `ui_splash_page.c` / `ui_checking_page.c`
  - 当前：外部 `tick()` + `is_done()` 轮询
  - 目标：使用 `sl_ui_post_delayed()` 实现超时自动跳转
  - splash: `on_resume()` 中注册 2 秒延迟 -> `sl_ui_navigate("checking")`
  - checking: 检测完成 -> `sl_ui_navigate("idle")`

### Phase 6: Service 接口层

- [ ] T6-1 定义 UI 可访问的 Service 接口
  ```c
  // app/ui_services.h
  int  params_service_get_dmx_addr(void);
  void params_service_set_dmx_addr(int addr);
  void params_service_commit(void);
  int  safety_service_get_tilt(void);
  void safety_service_set_tilt(int enable);
  int  lang_service_get(void);
  void lang_service_set(int lang);
  machine_state_t machine_service_get_state(void);
  uint8_t machine_service_get_pressure(void);
  // ...
  ```

- [ ] T6-2 Service 实现连接到 `g_app.params` 等业务对象
  - 遵循现有 `AGENTS.md` 规则：Service 不接受 UI 类型，返回 0 成功/负数失败

### Phase 7: 简化 freertos_app.c

- [ ] T7-1 移除所有 `ui_xxx_page_consume_xxx()` 调用
- [ ] T7-2 移除所有 `ui_xxx_page_update()` 推送调用
  - 改为页面 `on_tick()` 中主动拉取
- [ ] T7-3 移除所有 `ui_xxx_page_tick()` 调用
  - 改为页面内部定时器
- [ ] T7-4 简化 `ui_task` 主循环
  ```c
  void ui_task(void *pvParameters) {
      ui_setup_once();
      for (;;) {
          button_ticks();
          sl_ui_run();
          vTaskDelay(pdMS_TO_TICKS(20));
      }
  }
  ```

## 4. 文件变更预估

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `core/inc/sl_page.h` | 修改 | 扩展 sl_Page 结构体（on_resume/on_pause/on_result/on_tick） |
| `core/inc/sl_page_manager.h` | 修改 | 新增 sl_ui_run/slu_i_tick_up/slu_i_navigate 等 API |
| `core/src/sl_page_manager.c` | 修改 | 实现注册表、导航、时基、生命周期 |
| `core/inc/sl_page_registry.h` | 新增 | 页面注册表接口 |
| `core/src/sl_page_registry.c` | 新增 | 页面注册表实现 |
| `core/inc/sl_nav_intent.h` | 新增 | 导航意图定义 |
| `app/ui_services.h` | 新增 | Service 接口声明 |
| `app/ui_services.c` | 新增 | Service 接口实现 |
| `app/ui_pages/ui_idle_page.c` | 修改 | on_tick 拉取数据，自主导航 |
| `app/ui_pages/ui_main_menu.c` | 修改 | 自主导航到目标页面 |
| `app/ui_pages/ui_language_page.c` | 修改 | 内部调用 service，set_result |
| `app/ui_pages/ui_safety_page.c` | 修改 | 内部调用 service，set_result |
| `app/ui_pages/ui_setting_page.c` | 修改 | 内部调用 service |
| `app/ui_pages/ui_splash_page.c` | 修改 | 使用 post_delayed 自动跳转 |
| `app/ui_pages/ui_checking_page.c` | 修改 | 使用 on_tick 检测 + 自动跳转 |
| `project/src/freertos_app.c` | 修改 | 大幅精简 ui_task |

## 5. 执行顺序

1. **Phase 1** (时基) — 独立，无依赖，风险最低
2. **Phase 2** (注册) — 独立，可与 Phase 1 并行
3. **Phase 3** (导航) — 依赖 Phase 2 注册表
4. **Phase 4** (生命周期) — 依赖 Phase 3 导航
5. **Phase 6** (Service) — 可与 Phase 4 并行
6. **Phase 5** (页面重构) — 依赖 Phase 3 + 4 + 6
7. **Phase 7** (精简 freertos_app) — 依赖 Phase 5 全部完成

## 6. 约束与风险

- **向后兼容**：Phase 1~4 期间，现有页面必须继续工作；新 API 与旧 API 共存
- **无动态内存**：注册表、导航栈均使用静态数组，遵循 C11 约束
- **ISR 安全**：`sl_ui_tick_up()` 在 ISR 中调用，必须极简（仅累加计数器）
- **线程安全**：`sl_ui_tick_up()` 写计数器，`sl_ui_run()` 读计数器，需考虑原子性
- **ROM/RAM 预算**：122×32 小屏 MCU，新增代码需控制在合理范围
- **渐进式迁移**：每个 Phase 完成后必须可编译可运行，不搞大爆炸重构
