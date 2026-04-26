# Flamethrower

基于 `AT32F415CBT7 + FreeRTOS` 的喷火机控制固件工程。

这个仓库不只是一个应用工程，也沉淀了两块可复用的中间件能力：

- `SlateUI`：面向 MCU 的轻量级图形界面库
- `easyDMX`：面向工业控制场景的 DMX512 接收与解析中间件

## 核心亮点

### SlateUI

`SlateUI` 位于 [middleware/SlateUI](middleware/SlateUI)，是本工程里重点打磨的 MCU UI 库，特点是：

- 纯 C 实现，静态内存分配
- 页面栈 + 事件驱动模型
- 支持 122x32 小屏场景
- 内置中英文字体与多语言切换
- 适合菜单页、设置页、状态页、告警页这类小屏设备 UI

在本项目中，`SlateUI` 已用于实现：

- 欢迎页
- 检测页
- 主界面
- 主菜单
- DMX 设置页
- 压力/延时设置页
- 语言切换页

更多说明见：

- [middleware/SlateUI/README_zh-CN.md](middleware/SlateUI/README_zh-CN.md)

### easyDMX

`easyDMX` 位于 [middleware/easyDMX](middleware/easyDMX)，是本工程自研的 DMX512 接收中间件，目标是做成可复用的工业级 DMX 基础库。

当前版本特性：

- 面向对象风格设计：`对象 = 结构体 + 接口函数`
- KFIFO 风格环形缓冲区
- 基于 UART 中断事件流接收
- BREAK / Start Code / Slot 帧解析
- 最新帧快照读取
- 在线状态判断
- 统计信息输出：短帧、长帧、非零起始码、FIFO 溢出等

在本项目中的链路是：

`USART1 IRQ -> bsp_uart -> easyDMX -> dmx_strategy -> control_task`

## 工程能力

当前工程已经覆盖以下主线能力：

- 上电参数加载与默认参数回退
- 压力采样与百分比换算
- 倾斜检测
- 故障管理：`E1 ~ E5`
- TEST / USER 模式控制
- DMX 2CH / 6CH 业务解析
- UI 菜单与参数设置
- 泵、点火器、锁油阀、泄压阀执行控制

## 目录结构

```text
Flamethrower/
|- app/                  应用层与 UI 页面
|- bsp/                  板级支持包
|- cfg/                  系统配置与参数换算
|- doc/                  需求、TODO、DMX 规则、设计文档
|- domain/               领域逻辑（状态机、故障、DMX 策略、安全裁决）
|- hal_if/               HAL 抽象接口
|- middleware/
|  |- SlateUI/           自研 MCU 图形界面库
|  |- easyDMX/           自研 DMX512 中间件
|  |- MultiButton/       按键库
|  `- RTT/               RTT 日志
|- middlewares/          FreeRTOS 等第三方中间件
`- project/              MDK 工程与启动文件
```

## 构建环境

- MCU：`AT32F415CBT7`
- IDE：`Keil MDK`
- RTOS：`FreeRTOS`
- 日志：`SEGGER RTT`

主工程文件：

- [project/MDK_V5/Flamethrower.uvprojx](project/MDK_V5/Flamethrower.uvprojx)

## 编译与烧录

1. 使用 Keil 打开 `project/MDK_V5/Flamethrower.uvprojx`
2. 选择目标芯片与下载器配置
3. 编译并烧录
4. 使用 RTT Viewer 查看启动、自检、UI、故障与 DMX 日志

## 需求与执行基线

当前工程执行以这三份文档为主：

- [AI.md](AI.md)
- [AI_CHECKLIST.md](AI_CHECKLIST.md)
- [doc/工程长任务TODO_REVIEW.md](doc/%E5%B7%A5%E7%A8%8B%E9%95%BF%E4%BB%BB%E5%8A%A1TODO_REVIEW.md)

DMX 规则唯一数据源：

- [doc/DMX控制图.png](doc/DMX%E6%8E%A7%E5%88%B6%E5%9B%BE.png)

客户需求文档：

- [doc/大喷火机客户给的需求.md](doc/%E5%A4%A7%E5%96%B7%E7%81%AB%E6%9C%BA%E5%AE%A2%E6%88%B7%E7%BB%99%E7%9A%84%E9%9C%80%E6%B1%82.md)

## 当前状态

当前仓库已经完成一轮较完整的工程整理与功能打通，重点包括：

- `easyDMX` 接入实际 UART 中断链路
- `SlateUI` 小屏界面可用化
- 启动欢迎页 / 检测页 / Idle 流程修正
- 菜单回环与按键误触发问题修正
- 压力映射修正为 `4mA -> 0%`、`19.5mA -> 100%`
- 倾斜输入逻辑按当前硬件口径适配

## 说明

这个仓库当前以 `master` 为主分支。
