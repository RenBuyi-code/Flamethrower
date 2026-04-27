# 当前工程代码审查与调用链简化建议

日期：2026-04-27

范围：本次只阅读和审查当前源码，不修改业务代码。重点查看 `app/`、`app/rules/`、`cfg/`、`interfaces/`、`bsp/at32f415/`、`project/src/` 中与启动、任务调度、安全、DMX、执行器、UI 相关的代码；厂商库、FreeRTOS、ST7920 第三方示例默认不纳入重构对象。

## 1. 总体判断

工程已经形成了比较好的基础分层：

- `app/rules/` 保存纯业务规则：状态机、故障管理、DMX 策略、安全裁决。
- `app/task_*` 保存 FreeRTOS 任务逻辑：DMX 搬运、安全监控、控制决策、UI。
- `bsp/at32f415/` 负责硬件细节，并通过函数指针绑定给上层。
- `actuator_task` 是当前唯一真正落 GPIO 输出的任务，这一点符合安全约束。

当前主要问题不是“功能散乱”，而是“状态所有权和事件镜像边界还不够清楚”。多个任务都能直接读写 `app_fsm_t` 内部状态，同时又通过 EventGroup 维护一份状态/故障镜像，导致调用链在阅读时需要在 `g_app`、EventGroup、Queue、UI snapshot 之间来回跳。

建议后续重构不要先大拆文件，而是先收敛所有权：谁能写状态、谁能写故障、谁能发执行器命令、EventGroup 只表达哪些事实。收敛后代码会自然变短，调用链也会更直。

## 2. 当前主调用链

### 2.1 启动链路

```text
main()
  -> wk_system_clock_config()
  -> wk_periph_clock_config()
  -> wk_debug_config()
  -> wk_nvic_config()
  -> wk_timebase_init()
  -> wk_gpio_config()
  -> wk_adc1_init()
  -> wk_usart1_init()
  -> wk_freertos_init()
       -> app_fsm_init()
            -> bsp_at32f415_bind()
            -> state_machine_init()
            -> fault_manager_init()
            -> event_log_init()
            -> cfg_get_default_params()
       -> app_fsm_load_or_default_params()
       -> app_run_rules_selftests()
       -> create queues/event group/easyDMX rx
       -> app_task_*_init()
       -> app_fsm_transition(MACHINE_SELFTEST)
       -> create FreeRTOS tasks
       -> vTaskStartScheduler()
```

入口清晰，但 `freertos_app.c` 目前同时承担了系统装配、领域自测、执行器任务、诊断任务四类职责，文件偏重。

### 2.2 DMX 输入到执行器输出链路

```text
USART1_IRQHandler()
  -> bsp_uart_dmx_irq_handler()
       -> bsp_uart_dmx_fifo_push_isr()

dmx_task()
  -> app->hal.dmx.poll_byte()
       -> bsp_uart_dmx_poll_event()
  -> edmx_rx_push_event()

control_task()
  -> edmx_rx_process()
  -> edmx_rx_is_online()
  -> edmx_rx_copy_latest()
  -> dmx_strategy_build_intent()
  -> safety_guard_eval()
  -> app_task_queue_send_latest(q_actuator, cmd)

actuator_task()
  -> xQueueReceive(q_actuator)
  -> apply timing rules
  -> g_app.hal.actuator.apply()
       -> hal_actuator_apply()
            -> GPIO output
```

这条链路方向是对的：ISR 只搬运，DMX 协议和业务决策都在任务侧完成，执行器由单任务统一输出。

### 2.3 安全监控链路

```text
safety_task()
  -> read ADC/input
  -> xQueuePeek(q_actuator_status)
  -> fault_manager_set()/fault_manager_try_clear()
  -> app_task_set_fault_bits()
  -> app_task_send_safe_off_high_prio()
  -> app_fsm_transition(FAULT/LOCKED)
```

安全链路能快速发 `SAFE_OFF`，但它和 `control_task` 同时修改故障与状态机，这是后续最值得收敛的地方。

### 2.4 UI 链路

```text
ui_task()
  -> read EventGroup / app->machine / app->faults / app->params / ADC / actuator_status
  -> ui_service_set_machine_snapshot()
  -> sl_ui_run_once()
  -> setting callbacks
       -> app->params.xxx = value
       -> cfg_sanitize_params()
       -> storage.save_params()
```

UI 页面通过 `ui_services` 和页面解耦是好方向。但 `ui_task` 仍直接读写 `app_fsm_t`，参数保存也直接改 `app->params`，后续可以变成显式的配置服务接口。

## 3. 主要改进点

### P0-1：明确 `app_fsm_t` 的写入所有权

现状：

- `control_task` 会直接 `fault_manager_set/try_clear(&app->faults, ...)`，并多处 `app_fsm_transition()`。
- `safety_task` 也会直接改 `app->faults` 和 `app->machine`。
- `ui_task` 直接修改 `app->params`。
- `actuator_task` 直接读 `g_app.hal`。

风险：

- `app->machine.current`、`app->faults.latched_mask`、`app->params` 跨任务共享，没有统一锁或单写者模型。
- 读代码时无法一眼判断状态变化来自哪个任务。
- EventGroup 是镜像，但有时又被当成输入事实来源，容易形成“真实状态到底在哪”的困惑。

建议：

- 短期：文档上先定义所有权。
  - `control_task`：唯一业务状态机写入者。
  - `safety_task`：故障检测者，只产出 fault report 或 safety event。
  - `ui_task`：只提交 config change，不直接改 `app->params`。
  - `actuator_task`：唯一执行器 GPIO 输出者。
- 中期：新增一个轻量 `app_runtime` 或 `app_model` 接口，封装状态/故障/参数更新；任务不再直接写 `app->machine`、`app->faults`、`app->params`。

收益：状态链路从“多个任务都能改”变成“事件输入 -> 单点归约 -> 状态输出”，阅读和验证都会轻很多。

### P0-2：把 EventGroup 从“状态镜像 + 输入事实 + 心跳”拆清楚

现状：

- EventGroup 同时承载机器状态、DMX 在线、故障位、心跳位。
- `app_task_set_state_bits()` / `app_task_set_fault_bits()` 每次先清后设。
- `control_task` 从 EventGroup 反推故障 mask，再喂给 `safety_guard_eval()`。
- `diag_task` 每 200ms 清心跳位，但 `EVT_HB_MASK` 不包含 `EVT_HB_DIAG_BIT`。

风险：

- 清/设 EventGroup 不是业务上的原子快照，读者可能读到中间态。
- 通过事件位反推故障 mask 会让 `app->faults` 和 EventGroup 有双事实源倾向。
- 心跳位、状态位、故障位混在一起，后续扩展时容易误清。

建议：

- 保留 EventGroup 只做“轻量通知/标志”：DMX online、heartbeat、UI redraw hint 等。
- 机器状态和故障 mask 从 `app_fsm_t` 或 `runtime_snapshot` 读取，不从 EventGroup 反推。
- 心跳建议独立成 `task_health_t` 或至少单独 helper，避免状态位和心跳位共用同一组操作函数。

### P0-3：`control_task` 过重，建议拆成“采样 -> 意图 -> 裁决 -> 命令”

现状：

`control_task()` 单函数包含启动自检、DMX 在线更新、DMX intent、输入采样、安全输入拼装、压力故障检测、TEST 模式、USER 模式、状态切换、命令发送。

建议先不拆任务，只拆静态函数：

```text
control_task()
  -> control_read_inputs()
  -> control_build_dmx_intent()
  -> control_build_safety_input()
  -> control_run_test_mode()
  -> control_run_user_mode()
  -> control_publish_state()
```

其中 `control_run_user_mode()` 可进一步返回一个结构：

```c
typedef struct {
  machine_state_t next_state;
  actuator_cmd_t cmd;
  bool send_to_front;
} control_decision_t;
```

收益：主循环能读成业务流程，而不是被细节淹没；后续做单元测试也更容易。

### P0-4：安全裁决输入不完整，且部分规则分散

现状：

- `safety_guard_eval()` 是单点裁决，但输入中的 `voltage_ok` 在 `control_task` 固定为 `1`。
- 压力传感器故障在 `control_task` 和 `safety_task` 都有检查。
- 倾斜、压力、DMX 离线、E1/E5 等规则分散在多个任务中。

建议：

- 把“传感器采样结果”收敛成一个 `machine_inputs_t`，包含压力 raw/pct、电压 ok、倾斜、user_mode、dmx_online、dmx_intent。
- `safety_guard_eval()` 只根据完整输入返回安全动作。
- 故障 latch 由一个模块根据输入更新，避免 `control_task` 和 `safety_task` 各自设同类故障。

这不是为了抽象而抽象，而是为了让安全链路可以被一屏读完、被一组测试覆盖。

### P1-1：执行器命令可以更语义化

现状：

`actuator_cmd_t` 只有四类：`SAFE_OFF`、`RELIEF`、`FIRE`、`PUMP_ONLY`，并附带延时和持续时间。`actuator_task` 内部保留 `fire_active/relief_active`，按命令解释输出。

建议：

- 保留当前命令队列，但补一个“命令来源/原因码”，例如 `ACT_REASON_DMX_FIRE`、`ACT_REASON_FAULT_SAFE_OFF`、`ACT_REASON_TEST_FIRE`。
- `priority` 当前只作为字段存在，队列发送主要靠 `to_front`，可二选一：要么删除 `priority`，要么让 helper 根据 priority 决定 send front/back。
- `ACT_CMD_FIRE` 在 TEST 模式下实际用于“点火器测试，锁油阀不开”，这个语义建议显式化为 `ACT_CMD_IGNITER_TEST` 或在 `user_mode=false` 时由 actuator 内部保证并记录。

收益：日志和安全审查会更清楚，尤其是区分“真喷火”和“点火测试”。

### P1-2：参数修改链路应从 UI 直接写改为提交请求

现状：

UI 保存回调直接写 `s_task_ui_cfg.app->params.xxx`，再落盘。

风险：

- `control_task` 同时读取这些参数，缺少快照或互斥。
- 设置 DMX 模式后，地址合法范围会变化，虽然 `cfg_sanitize_params()` 会修正，但 UI shadow 值和真实值可能短时不同。

建议：

- 新增 `app_config_update_*()` 或 `app_config_apply(system_params_t patch)`。
- UI 修改 shadow，保存时提交完整参数快照，由配置服务 sanitize、落盘、发布新快照。
- `control_task` 每轮读取参数快照，而不是直接读可变的 `app->params`。

### P1-3：`freertos_app.c` 职责过多

现状：

`freertos_app.c` 同时包含：

- 静态资源创建。
- 参数提交。
- UI 性能计时。
- 看门狗。
- 领域自测。
- `actuator_task`。
- `diag_task`。
- 任务装配。

建议文件拆分顺序：

1. `app/task_actuator/task_actuator.c`：迁出 `actuator_task`。
2. `app/task_diag/task_diag.c`：迁出 `diag_task` 和心跳检查。
3. `app/app_selftest.c`：迁出领域自测。
4. `project/src/freertos_app.c` 只保留 RTOS 资源创建和依赖注入。

这样不会改变功能，但能让工程入口明显变薄。

### P1-4：目录命名可改得更常规

现状：

- `app/rules/` 表达“业务领域层”，架构上说得通，但在 MCU 工程里不算最常见。
- `interfaces/` 中的 `if` 是 `interface` 的缩写，但容易和 C 语言 `if` 关键字混淆。
- `adc_if.h`、`input_if.h` 这类 `i_` 前缀更像其他语言的接口命名风格，在 C 工程里可读性一般。
- `interfaces/interface_types.h` 混放了硬件接口类型和业务类型，例如 `system_params_t`、`dmx_intent_t`。

建议：

- `interfaces/` 后续统一改名为 `interfaces/`。
- `app/rules/` 不建议改成顶层 `core/`。`core` 过泛，别人仍然看不出里面是应用规则、系统核心还是底层核心。
- 更推荐把当前 `app/rules/` 内容迁到 `app/rules/`，表示“应用规则，不直接依赖硬件和 RTOS”。
- `machine_state` 后续建议改名为 `state_machine`。当前它本质是状态转换规则，不是“状态值本身”。
- 当前 `app_fsm` 后续建议收敛为 `app_fsm`。`app_fsm` 表达“应用整机有限状态机”，比 `app_fsm` 更准确，也允许它作为业务编排中心耦合 DMX、安全、故障、压力、模式等业务概念。
- 接口文件改为更直白的名字：
  - `adc_if.h` -> `adc_if.h`
  - `input_if.h` -> `input_if.h`
  - `actuator_if.h` -> `actuator_if.h`
  - `dmx_rx_if.h` -> `dmx_rx_if.h`
  - `storage_if.h` -> `storage_if.h`
- `interface_types.h` 后续拆分：
  - 与硬件接口强相关的 `sensor_id_t`、`input_id_t` 可放在 `interfaces/interface_types.h`。
  - 业务类型如 `system_params_t`、`dmx_intent_t` 应迁到 `app/app_types.h` 或对应业务模块头文件。

注意：

- 目录改名会牵动 include 路径和 Keil 工程文件，建议放在控制链路轻拆之后执行。
- 第一次改名只做机械迁移，不夹带业务逻辑变化。
- 推荐目标结构：

```text
app/
  app_fsm.*       应用整机状态机：推进整机状态，调用规则，输出控制决策
  rules/          应用规则：状态机、故障管理、安全裁决、DMX 策略
  task_control/   FreeRTOS 控制任务
  task_safety/    FreeRTOS 安全任务
  task_dmx/       FreeRTOS DMX 搬运任务
  task_ui/        FreeRTOS UI 任务
  ui_pages/       UI 页面
interfaces/       上层依赖的硬件/平台接口，仅声明接口
bsp/at32f415/     interfaces 的 AT32F415 实现
```

额外观察：

- 当前 `machine_state` 自身还比较纯，只保存状态集合与合法转换。
- 真正的高耦合点在 `app_fsm_t`：它同时聚合 HAL、状态机、故障管理、事件日志、参数，并被 `control_task`、`safety_task`、`ui_task`、`actuator_task` 共同使用。
- 后续应把 `app_fsm_t` 从“大共享上下文”收敛为 `app_fsm_t` 一类业务状态机对象。`app_fsm` 可以耦合业务规则，但不要直接读写 GPIO、ADC、FreeRTOS Queue/EventGroup；这些副作用仍由任务层和 BSP 层完成。

### P1-5：注释编码与文档编码需要统一

现状：

部分源码和 README 在当前环境显示为乱码，`AI.md` 和部分 `doc/*.md` 是可读 UTF-8。

建议：

- 新增或统一约定：所有项目自有 `.c/.h/.md` 使用 UTF-8 without BOM。
- 先修 README 和自有源码注释，不动第三方库。
- 若 Keil 编译链对 UTF-8 注释无影响，建议一次性转换；若担心风险，先只转换文档和新增注释。

收益：后续 AI/人工审查质量会明显提升，尤其是安全注释和需求映射。

## 4. 建议的重构顺序

### 第一步：先定边界，不改行为

- 写入所有权约定：
  - 状态机：只允许一个模块写。
  - 故障 latch：只允许一个模块写。
  - 参数：UI 不直接写核心结构。
  - 执行器：保持 actuator_task 唯一 GPIO 输出。
- 梳理 EventGroup 位用途，标注哪些是事实、哪些是通知、哪些是心跳。

### 第二步：轻拆 `control_task`

目标不是换架构，而是先把主循环压缩成清晰流程。建议只抽静态函数，不改任务数量，不改队列协议。

### 第三步：把安全输入做成快照

引入一个输入快照结构，让 `safety_guard_eval()` 拿到完整上下文。先让 control 生成快照，再考虑 safety 独立上报。

### 第四步：迁出 actuator/diag/selftest

这是低风险的文件级整理。做完后 `freertos_app.c` 会更像“装配根”，调用链也更容易画清楚。

### 第五步：配置服务化

把 UI 参数保存从直接写 `app->params` 改为提交完整参数快照。这个改动涉及 UI、配置、存储，建议等状态所有权收敛后再做。

## 5. 可以保留的设计

- ISR 到 BSP FIFO，再由 `dmx_task` 喂 easyDMX，这条链路值得保留。
- `app/rules/dmx_strategy.c` 的纯函数设计很好，适合继续扩充边界测试。
- `safety_guard_eval()` 作为单函数安全裁决入口是好方向，只需要补全输入和减少外围重复规则。
- `actuator_task` 统一落 GPIO 的约束应继续保持。
- `ui_services` 隔离 UI 页面与核心状态的方向是对的，后续可以把它扩展成正式的只读快照服务。

## 6. 本次未做的事

- 未修改任何现有业务代码。
- 未执行 Keil 编译或板级验证。
- 未核对 `doc/DMX控制图.png` 与 CSV 的全部阈值，仅基于当前源码和已有 TODO 文档做结构审查。
- 未检查第三方库内部实现。

## 7. 推荐下一步

建议下一轮先做一个“小而稳”的重构：只拆 `control_task` 内部静态函数，并增加 `control_decision_t`，不改变任务、队列、状态机和执行器行为。这个收益最大，风险相对最小，也能为后续单元测试和安全审查打底。
