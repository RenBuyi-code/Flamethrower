# 调用链简化重构执行计划

日期：2026-04-27

目的：在不改变现有功能语义和安全策略的前提下，逐步让代码调用链更短、更清晰，尤其收敛共享状态写入、任务职责和 EventGroup 用途。

本计划承接 `doc/code_review_simplify_call_chain.md`。当前阶段仍以文档和执行边界为主；进入代码阶段时，应按本文顺序小步提交。

## 1. 重构总原则

- 安全优先：任何阶段都不能削弱 `SAFE_OFF` 快速下发能力。
- 单步小改：每一阶段只解决一个结构问题，避免行为重构和文件搬迁混在一起。
- 先收敛所有权，再抽象接口：先明确谁能写，再决定封装成什么 API。
- 保持任务数量不变：第一轮不新增/删除 FreeRTOS 任务。
- 保持执行器单写者：GPIO 输出仍只能经过 `actuator_task -> hal.actuator.apply()`。
- 保持 DMX 接收链路：`USART IRQ -> bsp_uart FIFO -> dmx_task -> easyDMX -> control_task` 不改方向。

## 2. 共享状态所有权约定

### 2.1 状态机 `app->machine`

目标所有者：`control_task`

允许：

- `control_task` 调用 `app_fsm_transition()`。
- `ui_task` 只读状态，用于显示。
- `diag_task` 不直接切状态，只发 `SAFE_OFF`。

待收敛：

- `safety_task` 当前会切 `MACHINE_FAULT` / `MACHINE_LOCKED`。后续应改为上报故障/安全事件，由 `control_task` 统一切状态。

### 2.2 故障锁存 `app->faults`

目标所有者：短期可由 `safety_task` 统一管理；中期迁移到独立 `fault_update` 模块。

允许：

- `safety_task` 根据传感器和执行器状态设置/清除 E1~E5。
- `control_task` 读取故障 mask 做控制决策。

待收敛：

- `control_task` 当前会设置 E1/E2/E4。后续应改为发出故障事件或复用统一故障更新函数。

### 2.3 参数 `app->params`

目标所有者：配置服务或 `app_fsm` 参数接口。

允许：

- `control_task` 读取参数快照。
- `ui_task` 提交参数修改请求。

待收敛：

- `ui_task` 当前直接写 `app->params.xxx`。后续改为 `app_config_apply()` 一类接口。

### 2.4 执行器输出

目标所有者：`actuator_task`

允许：

- 其他任务只能发送 `actuator_cmd_t`。
- `actuator_task` 唯一调用 `hal.actuator.apply()`。

当前状态符合约束，应继续保持。

## 3. EventGroup 用途边界

当前 EventGroup 混合了三类内容：

- 机器状态位：`EVT_STATE_*`
- 故障位：`EVT_FAULT_*`
- 通知/在线/心跳：`EVT_DMX_ONLINE_BIT`、`EVT_HB_*`

目标边界：

- EventGroup 只保留轻量通知和心跳。
- 状态和故障以 `runtime_snapshot` 或 `app_fsm` 只读接口作为事实源。
- UI 显示使用快照，不从 EventGroup 拼业务状态。

过渡策略：

1. 先不删除状态位/故障位，避免一次改动过大。
2. 新增只读快照接口后，让 UI 和 control 优先读快照。
3. 确认无依赖后，再减少 EventGroup 中的状态/故障镜像。

## 4. 分阶段执行方案

### 阶段 0：文档与验收线

状态：已开始。

改动范围：

- `doc/code_review_simplify_call_chain.md`
- `doc/call_chain_refactor_execution_plan.md`

不改：

- 不改 `.c/.h` 业务代码。
- 不改 Keil 工程文件。

验收：

- 有清晰调用链说明。
- 有共享状态所有权说明。
- 有后续代码阶段的改动边界和验收标准。

### 阶段 1：轻拆 `control_task` 主循环

目标：让 `control_task()` 主循环从“细节堆叠”变成“业务流程”。

建议改动文件：

- `app/task_control/task_control.c`

建议新增静态结构：

```c
typedef struct
{
  TickType_t now;
  EventBits_t bits;
  bool user_mode;
  bool menu_active;
  bool key_menu;
  bool key_down;
  bool key_up;
  bool key_enter;
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  bool tilt_fault;
  bool dmx_online;
} control_inputs_t;

typedef struct
{
  actuator_cmd_t cmd;
  machine_state_t next_state;
  uint16_t event_code;
  BaseType_t to_front;
  bool send_cmd;
  bool switch_state;
} control_decision_t;
```

建议抽出的静态函数：

- `control_init_safe_cmd()`
- `control_read_inputs()`
- `control_update_dmx_online()`
- `control_build_dmx_intent()`
- `control_build_safety_input()`
- `control_handle_pressure_sensor_fault()`
- `control_decide_test_mode()`
- `control_decide_user_mode()`
- `control_apply_decision()`

不改：

- 不改 `actuator_cmd_t` 字段。
- 不改 DMX 阈值。
- 不改 FreeRTOS 任务和优先级。
- 不改状态机规则。

验收：

- `control_task()` 主循环能清楚读成：采样 -> DMX 意图 -> 安全输入 -> 模式决策 -> 应用决策。
- 行为等价：原有 `SAFE_OFF`、`RELIEF`、`FIRE`、`PUMP_ONLY` 发送条件不变。
- 编译通过。
- 至少复核 DMX 失联、压力传感器故障、USER/TEST 模式三条安全路径。

风险点：

- 抽函数时遗漏 `last_test_action`、`pressure_refill_active` 这种跨轮状态。
- `ok` 当前既表示 DMX frame 有效，又参与 TEST 模式 DMX 控制，拆分时要命名清楚。

### 阶段 2：安全输入快照化

目标：把散落在 `control_task` 中的安全输入拼装显式化。

建议改动文件：

- `app/task_control/task_control.c`
- 可选：新增 `app/app_runtime_snapshot.h`

建议结构：

```c
typedef struct
{
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  bool pressure_sensor_fault;
  bool voltage_ok;
  bool tilt_fault;
  bool user_mode;
  bool dmx_online;
  dmx_intent_t dmx_intent;
  uint32_t fault_mask;
} machine_inputs_t;
```

不改：

- 不移动故障所有权。
- 不改变 `safety_guard_eval()` 对外接口，除非已准备好同步自测。

验收：

- `safety_eval_input_t` 的填充集中在一个函数。
- `voltage_ok` 不再硬编码为 `1`，而是明确来自配置/采样策略。若电压保护仍关闭，应通过 `cfg_voltage_raw_in_range()` 体现。

### 阶段 3：故障写入收敛

目标：避免 `control_task` 和 `safety_task` 同时写同类故障。

建议改动文件：

- `app/task_safety/task_safety.c`
- `app/task_control/task_control.c`
- 可选：新增 `app/app_fault_service.c/.h`

过渡方案：

- 保留 `safety_task` 作为故障 latch 主要写入者。
- `control_task` 对启动自检中的即时故障可以先保留，但普通循环中的压力传感器故障逐步转给统一故障更新。

验收：

- 普通运行循环中，E1/E2/E3/E5 由一个位置负责设置/清除。
- `control_task` 只根据故障 mask 决策，不直接重复设置同类故障。
- 故障触发后仍能高优先级发送 `SAFE_OFF`。

### 阶段 4：参数修改服务化

目标：UI 不再直接写 `app->params`。

建议改动文件：

- `app/task_ui/task_ui.c`
- `app/app_fsm.c`
- `app/app_fsm.h`

建议接口：

```c
bool app_fsm_apply_params(app_fsm_t *core, const system_params_t *params);
bool app_fsm_get_params_snapshot(const app_fsm_t *core, system_params_t *out);
```

验收：

- UI 保存只提交完整参数快照。
- sanitize 和 storage save 集中在一个接口里。
- DMX mode 切换后，地址 shadow 能同步到 sanitize 后的真实值。

### 阶段 5：文件级迁移

目标：让 `freertos_app.c` 回到装配根职责。

建议迁移顺序：

1. `actuator_task` -> `app/task_actuator/task_actuator.c`
2. `diag_task` -> `app/task_diag/task_diag.c`
3. app/rules selftest -> `app/app_selftest.c`

验收：

- 迁移后任务创建 API 不变。
- Keil 工程文件同步包含新文件。
- `freertos_app.c` 只保留资源创建、依赖注入、任务创建和调度器启动。

### 阶段 6：目录命名整理

目标：把不够常规的 `interfaces/` 命名整理为更直观的接口层命名，并把应用纯规则放回 `app/` 目录体系内。

建议改动：

```text
interfaces/        -> interfaces/
adc_if.h        -> adc_if.h
input_if.h      -> input_if.h
actuator_if.h   -> actuator_if.h
dmx_rx_if.h        -> dmx_rx_if.h
storage_if.h    -> storage_if.h
interface_types.h      -> interface_types.h 或拆分

app/rules/        -> app/rules/
machine_state  -> state_machine
app_fsm       -> app_fsm
```

命名原则：

- 统一使用 `interfaces/` 表达“上层依赖的硬件/平台接口”。
- 文件名使用 `xxx_if.h`，避免 `i_` 前缀和 `interfaces` 缩写。
- 如果暂时不拆 `interface_types.h`，可先机械改名为 `interface_types.h`；后续再把业务类型拆出去。
- 不把 `app/rules/` 改成顶层 `core/`。`core` 太泛，不如 `app/rules/` 直观。
- `app/rules/` 表达“应用规则”，与 `app/task_*` 的任务编排分开。
- `state_machine` 只保存状态集合与合法转换，不做状态进入/退出动作。
- 当前更大的耦合点是 `app_fsm_t`，它聚合 HAL、状态机、故障、事件和参数。后续不要继续保留“大共享上下文”语义，而是收敛为 `app_fsm`。
- `app_fsm` 表达“应用整机有限状态机”，允许耦合业务规则，但不直接耦合硬件寄存器和 RTOS 原语。

不改：

- 不改变接口结构体字段。
- 不改变 BSP 绑定方式。
- 不改变业务逻辑。

验收：

- 全工程 include 路径更新完整。
- Keil 工程文件包含新路径。
- `app/rules` 仍不直接依赖芯片寄存器和 FreeRTOS 类型。
- 纯机械改名后编译通过。

### 阶段 7：`app_fsm` 收敛为 `app_fsm`

目标：把当前“全局共享大对象”改造成明确的应用整机状态机。

建议方向：

```text
app_fsm_t
  -> app_fsm_t

app_fsm_init()
  -> app_fsm_init()

app_fsm_transition()
  -> app_fsm_transition()
```

边界：

- `app_fsm` 可以持有状态机、故障、参数快照、事件日志等业务状态。
- `app_fsm` 可以调用 `app/rules/*` 里的纯规则。
- `app_fsm` 不直接读 ADC/GPIO，不直接写执行器 GPIO。
- `app_fsm` 不直接操作 FreeRTOS Queue/EventGroup。
- 任务层负责采样输入、调用 `app_fsm_step()`、发送执行器命令、发布 UI/事件通知。

理想调用链：

```text
control_task()
  -> read inputs
  -> app_fsm_step(&fsm, &input, &output)
  -> send actuator command
  -> publish snapshot/events
```

验收：

- `app_fsm` 这个命名不再作为业务中心出现。
- 整机状态推进入口集中到 `app_fsm`。
- 任务层和硬件层副作用没有被塞进 `app_fsm`。

## 5. 代码阶段每次提交前检查

- 是否仍只有 `actuator_task` 调用 `hal.actuator.apply()`。
- 是否仍能在故障/DMX 离线/心跳异常时发 `SAFE_OFF`。
- 是否新增了未保护的跨任务共享写入。
- 是否让 EventGroup 承担了新的业务事实源职责。
- 是否修改了 DMX 阈值或安全阈值；若修改，必须映射需求文档。
- 是否保持 `app/rules/` 不依赖芯片寄存器和 FreeRTOS 类型。

## 6. 建议的下一次实际代码动作

优先做阶段 1：轻拆 `app/task_control/task_control.c`。

理由：

- 风险低，只做函数抽取。
- 收益高，主控制链路会明显变短。
- 不需要改任务、队列、HAL、DMX 协议和执行器输出。
- 为后续安全快照和故障所有权收敛铺路。

最小可交付范围：

- 新增 `control_inputs_t` 和 `control_decision_t`。
- 抽出 DMX intent 构建、安全输入构建、TEST 决策、USER 决策、决策应用。
- 保持原有日志和事件码尽量不变。
- 编译或至少做静态阅读自检。
