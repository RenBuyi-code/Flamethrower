# Flamethrower

## 基于 `AT32F415CBT7 + FreeRTOS` 的喷火机控制固件工程。

## 概览

DMX512 协议驱动的喷火机控制系统，覆盖从协议解析、安全监控到执行器控制的完整闭环：

- **DMX 接收与解析**：支持 2CH / 6CH 动态切换
- **安全监测**：压力、电压、倾斜多维度故障检测（E1\~E5），故障锁存与安全关断
- **控制决策**：多状态机，协调点火、喷射、泄压等动作序列
- **UI 系统**：小屏 OLED 参数配置与实时状态展示，支持多语言
- **自研中间件**：SlateUI（轻量级嵌入式 UI 框架）、easyDMX（DMX512 协议解析库）
- **任务调度**：FreeRTOS 多任务协同，事件标志组驱动模块间通信

***

## 架构设计

### 分层

```
interfaces/      硬件接口抽象（UART、GPIO、ADC 等 HAL 定义）
bsp/             板级实现（AT32F415 外设驱动）
app/rules/       业务规则（状态机、故障管理、安全裁决、DMX 策略）
app/task_*       RTOS 任务编排与执行器控制
project/         启动装配与依赖注入
```

***

### 关键调用路径

```
DMX:       USART1 IRQ → bsp_uart → task_dmx → easyDMX → task_control → task_actuator → HAL apply

Safety:    task_safety(采样+判定) → fault latch → SAFE_OFF → state machine

UI params: task_ui(draft) → app_fsm_get_params_snapshot → mutate → app_fsm_apply_params(sanitize+persist)
```

***

### 参数更新链路（单写者约束）

```
UI task (草稿)
   → app_fsm_get_params_snapshot()  // 读取完整快照
   → mutator function                // 修改单项
   → app_fsm_apply_params()          // 校验、持久化、生效
```

所有参数写入必须经过 `app_fsm_apply_params()`，UI 和 DMX 不直接操作参数存储。

***

## 项目结构

```text
app/                    # 应用层
├── app_fsm.*           # 核心状态机 + 参数管理
├── app_selftest.*      # 启动自检
├── app_task_common.*   # 任务间通信组件
├── rules/              # 业务规则模块
│   ├── state_machine.*
│   ├── fault_manager.*
│   ├── safety_guard.*
│   ├── dmx_strategy.*
│   └── event_log.*
├── task_control/       # 控制任务
├── task_safety/        # 安全监测任务
├── task_actuator/      # 执行器控制任务
├── task_dmx/           # DMX 接收任务
├── task_ui/            # UI 任务
├── task_diag/          # 诊断任务
├── ui_pages/           # UI 页面组件
├── ui_services.*       # UI 与业务层桥接
└── log_rtt.h
interfaces/             # HAL 接口定义
bsp/                    # AT32F415 板级实现
cfg/                    # 系统配置与参数校验
middleware/             # 自研中间件
├── SlateUI/            # 轻量级嵌入式 UI 框架
├── easyDMX/            # DMX512 协议解析库
├── MultiButton/        # 按键状态机库
└── RTT/                # SEGGER RTT 日志
project/MDK_V5/         # Keil 工程
```

***

## 自研中间件

### SlateUI — 嵌入式 UI 框架

面向资源受限 MCU 的 UI 框架，围绕四条设计主线展开。

#### 对象模型：控件树 + 函数指针（虚函数表）

`sl_Widget` 基类通过函数指针（draw/proc，类似 C++ 的虚函数表）实现不同类型控件的差异化行为，控件以 first-child / next-sibling 构成树形结构：

```c
typedef struct sl_Widget {
    sl_widget_id_t   id;            // 控件标识符
    int16_t          x, y, w, h;   // 位置与尺寸
    struct sl_Widget *parent;
    struct sl_Widget *first_child;  // 子控件链表头
    struct sl_Widget *next_sibling; // 兄弟控件链表
    sl_WidgetDraw    draw;          // 绘制虚函数
    sl_WidgetProc    proc;          // 事件处理虚函数
    uint8_t          flags;         // VISIBLE / FOCUSABLE / FOCUSED
    void            *user_data;
} sl_Widget;
```

- 递归遍历：`sl_widget_draw_tree()` 深度优先绘制，父级偏移量逐层累加
- 事件路由：`sl_widget_dispatch_event()` 深度优先分发，第一个消费事件的控件终止传递
- 控件查找：`sl_widget_find_by_id()` 支持 ID 定位，用于跨控件通信

具体控件（Label、ListView、Icon、ProgressBar 等）将 `sl_Widget` 作为第一个成员，通过地址强制转换复用基类逻辑（C 语言中常见的继承惯用法）。

#### 事件系统：两层管道

```
按键 GPIO → sl_event_post() (原始事件队列, 环形缓冲区)
                  ↓
            sl_Widget.proc()  (控件树内分发)
                  ↓
            sl_ui_event_post() (UI 语义事件, 多订阅者广播)
                  ↓
            Presenter 回调 (消费端)
```

- **原始事件**：静态环形缓冲区（SL\_EVENT\_QUEUE\_SIZE=16），ISR 安全，无动态分配
- **语义事件**：回调数组，起/停/值变更等 UI 语义由控件产生，Presenter 订阅消费
- 两层的意义：原始事件与业务逻辑之间插入语义抽象层。控件只负责发出"确认键被按下"这一事实（`SL_UI_EVT_ENTER_ITEM`），至于按下后是跳转页面还是提交参数，由外层 Presenter 回调决定。业务行为变更时只需替换回调注册，无需修改控件代码。

#### 页面系统：生命周期 + 注册表

页面以 `{name, getter}` 条目注册，导航时由框架创建并驱动生命周期：

```
enter → init → [tick → proc → draw] 循环 → exit
```

```c
sl_Page *my_page_get(void) {
    static sl_Page p = {
        .name = "my_page",
        .init = my_page_init,
        .draw = my_page_draw,
        .proc = my_page_proc,
        .exit = my_page_exit,
        .tick = my_page_tick,
    };
    return &p;
}

// 注册 → 初始化 → 主循环
sl_ui_register_pages(pages, count);
sl_ui_init("splash");

// 定时器中断中（固定周期调用）
void timer_isr(void) {
    sl_ui_tick_up();
}

// 主循环
while (1) {
    sl_ui_run_once();
}
```

- `arg` 字段支持页面间传参（`sl_page_enter_with("page", arg)`）
- `data` 字段由页面自身分配私有状态，框架不介入生命周期

#### 端口抽象层

`port/sl_port.c` 集中封装硬件依赖，框架核心不直接调用 MCU HAL：

| port 接口                              | 用途          |
| ------------------------------------ | ----------- |
| `sl_disp_init()` / `sl_disp_flush()` | 显示初始化与刷新    |
| `sl_disp_*` 绘制原语                     | 像素、矩形、文本、位图 |
| `sl_hw_delay_ms()`                   | 延时（初始化阶段）   |
| `sl_port_input_init()`               | 输入设备初始化     |

#### 菜单框架

`menu/` 子模块提供声明式菜单模型（`sl_MenuModel`），通过键值对描述菜单结构，由 `MenuPage` 统一渲染，无需为每个菜单单独编写页面逻辑。

#### 内存策略

全静态分配，无 `malloc`。事件队列、页面注册表、控件树均在编译期确定上限，运行时零动态内存开销。

### easyDMX — DMX512 协议解析库

两级 FIFO 设计：BSP 层环形缓冲区存储 `{byte, is_break}` 事件，easyDMX 层消费后经状态机组装为完整 DMX 帧。

- **零拷贝**：事件通过 FIFO 传递，不复制原始数据
- **状态机解析**：Break（帧起始中断）→ MAB（中断后标志）→ Slot（时隙）→ Idle（空闲）状态迁移，支持帧同步恢复
- **在线检测**：基于接收时间戳判定 DMX 信号是否丢失
- **统计接口**：暴露丢帧计数、字节丢弃数等诊断数据

```c
edmx_rx_t rx;
edmx_rx_init(&rx, fifo_storage, sizeof(fifo_storage), timeout_ms);
edmx_rx_push_event(&rx, &evt);       // ISR / 轮询侧
edmx_rx_process(&rx, now_ms);         // 解析侧
edmx_rx_copy_latest(&rx, &frame);     // 获取最新帧
edmx_rx_is_online(&rx, now_ms);       // 在线状态
```

***

## 演示

<video src="res/video.mp4" controls width="100%"></video>

*开机展示与语言切换演示*

<video src="res/video2.mp4" controls width="100%"></video>

*按键加速演示：长按上下键快速调整参数，SlateUI 按键重复机制*

***

## 构建

- MCU: AT32F415CBT7 (Cortex-M4F, 128KB Flash, 32KB SRAM)
- IDE: Keil MDK
- RTOS: FreeRTOS
- Log: SEGGER RTT

打开 `project/MDK_V5/Flamethrower.uvprojx` → Build → Flash → RTT Viewer 查看日志。

***

## 开发协作

本项目是一个**纯 AI 工程**：全部代码由 AI 生成，人未直接编写一行代码。人的角色是架构师——负责系统设计、规则约束、质量审查与决策，AI 负责按指令实现。

主力模型：Codex 5.3 与 ChatGPT 5.4/5.5（代码生成以及调试、架构讨论与文档，合计消耗约一周 Plus 会员额度）、deepseek-v4-flash 与 deepseek-v4-pro（README 文档编写与代码生成以及调试，合计消耗 127.3M tokens，约 31 CNY，其中 pro 失败率较高导致无效 token 占比较大）。

***

### 为什么这样做

嵌入式固件开发中，大量工作属于"确定性的编码实现"：按照接口定义写函数体、按照状态机画 switch-case、按照通信协议写解析逻辑。这些工作重复性高、容错窗口小，但恰恰是 AI 擅长的事情。

人的精力应该放在更高价值的事情上：系统该怎么分层、模块之间的边界在哪里、安全红线怎么划、验收标准定到什么程度。这些决策决定了项目最终的质量天花板，AI 无法替代。

基于这个判断，形成了如下分工。

***

### 分工

```
 ┌─────────────────────────────────────────────────────────────┐
 │                    人（架构师）                               │
 │                                                             │
 │  · 系统分层设计：interfaces / bsp / app/rules / task_*       │
 │  · 接口契约定义：每个模块提供什么能力、依赖什么能力           │
 │  · 业务规则建模：状态迁移图、故障分类（E1~E5）、安全策略       │
 │  · 质量红线划定：哪些事情绝不允许（ISR 含业务逻辑等）          │
 │  · 验收标准制定：什么样的交付算"做完"                         │
 │  · Review 与合入决策：逐项核对清单，决定是否通过              │
 └───────────────────────┬─────────────────────────────────────┘
                         │ 口头/文字需求 + 确认 + 反馈
                         ▼
 ┌─────────────────────────────────────────────────────────────┐
 │                    AI（执行层）                               │
 │                                                             │
 │  · 将人的需求整理为需求文档和 TODO 清单                       │
 │  · 按接口定义编写模块实现代码                                │
 │  · 按状态机/策略文档编写业务规则代码                          │
 │  · 编写 RTOS 任务与任务间通信代码                             │
 │  · 编写自研中间件（SlateUI、easyDMX）                        │
 │  · 按 Review 意见修正代码                                    │
 │  · 输出改动清单、风险点说明、测试结果                         │
 └─────────────────────────────────────────────────────────────┘
```

***

### 协作流程

每一轮改动遵循固定的六步流程：

```
Step 1  人（架构师）
        ↓  提出需求：要做什么、什么时机做、验收标准
Step 2  AI
        ↓  整理成需求文档 / TODO 清单 / 接口定义，交人确认
Step 3  人（架构师）
        ↓  确认文档和方向无误
Step 4  AI
        ↓  按清单实现代码，输出改动说明与风险点
Step 5  人（架构师）
        ↓  执行 Review，核对清单，确认是否通过
Step 6  人（架构师）
        合入或退回修正
```

每轮交付以 **"可编译、可回退、有解释"** 为验收标准：

- **可编译**：交付代码必须通过编译，不可留下语法错误让下一个环节擦屁股
- **可回退**：单轮改动边界清晰，发现问题能精准回退而不影响其他模块
- **有解释**：AI 必须输出变更说明和风险点，不能只扔代码不说话

***

### 质量约束体系

AI 编写代码自由度大，必须用规则框住。项目的质量约束通过 `AI_CHECKLIST.md` 落地，AI 在执行前后必须逐项勾选：

**改动前（Pre-Change）**

- 确认本次改动的需求基线，与已有 TODO 清单对应
- 识别影响面：涉及哪些状态机状态？哪些故障路径？哪些 RTOS 任务和事件组？
- 确认改动不触碰第三方代码（middlewares/），若必须触碰需说明理由

**改动中（In-Change）**

- 业务规则层（app/rules/）不得直接依赖寄存器或芯片库类型
- 执行器写入口必须收敛到 `actuator_task`，禁止多任务直写 GPIO
- ISR 仅做最小工作（采样 / 搬运 / 通知），不做业务决策
- 所有外部输入（DMX / ADC / 按键 / 参数）必须做边界校验
- 参数变更必须有默认值与越界回退策略

**改动后（Post-Change）**

- 输出改动文件清单
- 输出风险点清单（无高风险也必须明确写出）
- 输出测试结果（已执行 / 未执行及原因）
- 输出残余风险（若有）

**否决项（一票否决）**
以下任一命中，Review 结论直接为"未通过"，退回修正：

- 未验证安全降级路径
- 执行器存在多任务直写 GPIO
- ISR 内含业务逻辑
- 外部输入未做边界校验
- 缺失 DMX 失联处理

***

### 文档驱动

每一次改动从人的需求开始，到文档归档结束：

```
人提出需求 → AI 整理成 TODO 与接口定义 → 人确认 → AI 实现 → AI 输出变更说明 → 人更新架构文档和复盘记录
```

`doc/` 目录维护了全生命周期的工程记录：

| 文档      | 用途                  |
| ------- | ------------------- |
| 架构审查记录  | 重构前的架构问题分析与方案评审     |
| 调用链重构方案 | 分步执行计划，每一步都标注目标和风险  |
| 分步复盘    | 每完成一步后的技术复盘，记录决策依据  |
| 回归验证清单  | 每次改动后回归验证，确保不引入回归缺陷 |
| 历史归档    | 已完成的阶段性版本记录         |

***

### 这套方式的收益

从结果来看，这种协作模式带来的实际效果：

- **架构一致性**：所有代码由一个架构意图贯穿，不存在多人协作时的风格断裂和设计偏离
- **文档完整度**：每一步都有文档对应，项目历史可追溯，新人上手成本低
- **质量可预期**：硬性红线挡住了一类常见嵌入式 bug（ISR 抢资源、无校验输入等）
- **交付节奏可控**：每轮改动范围小、边界清晰、可独立验证，不会出现"改一处崩一片"

## 相关文档

| 文档                                          | 用途      |
| ------------------------------------------- | ------- |
| `doc/code_review_simplify_call_chain.md`    | 架构审查记录  |
| `doc/call_chain_refactor_execution_plan.md` | 调用链重构方案 |
| `doc/refactor_step_reviews.md`              | 分步重构复盘  |
| `doc/regression_checklist_ai_driven.md`     | 回归验证清单  |
| `doc/history/`                              | 历史版本归档  |
| `AI.md`、`AI_CHECKLIST.md`                   | 人机协作基线  |

