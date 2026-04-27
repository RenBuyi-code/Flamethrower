# 重构步骤 Review 记录

日期：2026-04-27

## Step 1：`hal_if` -> `interfaces`

改动范围：

- 新增目录 `interfaces/`。
- 文件重命名：
  - `hal_if/i_adc.h` -> `interfaces/adc_if.h`
  - `hal_if/i_input.h` -> `interfaces/input_if.h`
  - `hal_if/i_actuator.h` -> `interfaces/actuator_if.h`
  - `hal_if/i_dmx.h` -> `interfaces/dmx_rx_if.h`
  - `hal_if/i_storage.h` -> `interfaces/storage_if.h`
  - `hal_if/i_types.h` -> `interfaces/interface_types.h`
- 接口类型重命名：
  - `i_adc_t` -> `adc_if_t`
  - `i_input_t` -> `input_if_t`
  - `i_actuator_t` -> `actuator_if_t`
  - `i_dmx_t` -> `dmx_rx_if_t`
  - `i_storage_t` -> `storage_if_t`
- 更新 `bsp/at32f415/bsp_at32f415.h`、`cfg/system_config.h` 和 Keil include path。

Review 结论：通过。

核对结果：

- 未改变接口结构体字段。
- 未改变 BSP 绑定逻辑。
- 未改变业务逻辑。
- `rg` 未发现旧路径 `hal_if`、旧头文件名 `i_*.h`、旧接口类型 `i_*_t` 残留于当前项目代码。

残余风险：

- 尚未执行 Keil 编译。
- `interface_types.h` 仍混放硬件接口类型和业务类型，后续需要拆分或迁移。

## Step 2：`domain` -> `app/rules`，`machine_state` -> `state_machine`

改动范围：

- 新增目录 `app/rules/`。
- 文件迁移：
  - `domain/dmx_strategy.*` -> `app/rules/dmx_strategy.*`
  - `domain/event_log.*` -> `app/rules/event_log.*`
  - `domain/fault_manager.*` -> `app/rules/fault_manager.*`
  - `domain/safety_guard.*` -> `app/rules/safety_guard.*`
  - `domain/machine_state.*` -> `app/rules/state_machine.*`
- 类型/函数重命名：
  - `machine_state_ctx_t` -> `state_machine_t`
  - `machine_state_init()` -> `state_machine_init()`
  - `machine_state_transition()` -> `state_machine_transition()`
- 保留 `machine_state_t` 枚举名，用于表达“一个机器状态值”。
- 更新 Keil 工程中的 include path 和 app rules 文件路径。

Review 结论：通过。

核对结果：

- 未改变状态枚举值。
- 未改变状态转换规则。
- 未改变 DMX 策略、安全裁决、故障管理、事件日志的业务逻辑。
- `rg` 未发现旧目录 `domain`、旧头文件 `machine_state.h/.c`、旧函数 `machine_state_*`、旧类型 `machine_state_ctx_t` 在项目源码内残留。
- 本步骤中曾发生一次批量替换范围过宽，误改到第三方示例/库和已有文档；已立即用 `git restore` 回滚误伤文件，仅保留本项目源码和本次新增文档的目标改动。

残余风险：

- 尚未执行 Keil 编译。
- `app/rules` 中部分注释仍是旧语义或编码显示异常，后续单独处理，不混入本次机械迁移。

## Step 3：`app_core` -> `app_fsm` 安全子集

改动范围：

- 文件重命名：
  - `app/app_core.c` -> `app/app_fsm.c`
  - `app/app_core.h` -> `app/app_fsm.h`
- 类型/函数重命名：
  - `app_core_t` -> `app_fsm_t`
  - `app_core_init()` -> `app_fsm_init()`
  - `app_core_load_or_default_params()` -> `app_fsm_load_or_default_params()`
  - `app_core_log()` -> `app_fsm_log()`
  - `app_core_switch_state()` -> `app_fsm_transition()`
- 更新 `task_*` 配置头、`freertos_app.c`、Keil 工程文件中的引用。

Review 结论：通过。

核对结果：

- 未改变 `app_fsm_t` 内部字段。
- 未改变状态转换、参数加载、事件记录逻辑。
- 未新增 `app_fsm_step()`，避免把行为重构混入本次机械命名。
- `rg` 未发现旧符号 `app_core`、`app_core_t`、`app_core_*` 在项目源码内残留。

残余风险：

- 尚未执行 Keil 编译。
- 局部变量和任务配置字段仍叫 `app`，没有统一改成 `fsm`。这是刻意保留，避免一次性修改过大。
- `app_fsm` 当前仍持有 HAL、状态、故障、事件、参数，仍是过渡形态；后续需要继续瘦身为真正的输入/输出式 FSM。

## Step 4：最终静态检查

检查项：

- `git diff --check`
- 旧命名残留扫描：
  - `hal_if`
  - `i_*_t`
  - `i_*.h`
  - `domain`
  - `machine_state_ctx_t`
  - `machine_state_init`
  - `machine_state_transition`
  - `machine_state.h/.c`
  - `app_core`
- Keil 工程 `FilePath` 存在性检查。
- 本机 Keil/ARM 编译命令探测。

Review 结论：通过，带残余风险。

核对结果：

- `git diff --check` 未报空白错误，仅提示 CRLF/LF 换行转换警告。
- 项目源码内未发现旧目录/旧符号残留。`machine_state_t` 保留是预期行为，用于表达机器状态枚举值。
- Keil `.uvprojx` 中列出的文件路径均能在当前工作区找到。
- 未发现可用的 `UV4/UV5/armcc/armclang` 命令，因此未执行真实 Keil 编译。

残余风险：

- 未执行目标编译，仍需在 Keil 中打开 `project/MDK_V5/Flamethrower.uvprojx` 做一次完整 build。
- 当前多处源文件被工具按 UTF-8 写回，Git 提示后续可能发生 CRLF/LF 规范化；建议编译通过后统一处理换行策略。
- `project/MDK_V5/Flamethrower.uvoptx` 与 `Flamethrower.ATWP` 在本轮开始前已处于修改状态，本轮未把它们作为主要改动目标。

补充记录：

- 用户已在 Keil 编译并反馈命名迁移相关报错清零。
- 曾出现 `app_run_app/rules_selftests()` 非法标识符报错，已修正为 `app_run_rules_selftests()`。

## Step 5: `control_task` light split

Scope:
- Kept the main user-mode FSM decision in `control_task`.
- Extracted repeated or preparatory logic into file-local helpers:
  - `task_control_heartbeat_delay()`
  - `task_control_update_dmx_online()`
  - `task_control_build_dmx_intent()`
  - `task_control_fill_safety_input()`
  - `task_control_init_actuator_cmd()`
  - `task_control_handle_pressure_sensor_fault()`
  - `task_control_choose_test_action()`
  - `task_control_send_test_action()`

Review:
- PASS with residual compile risk.
- Runtime operation order is preserved: process DMX, update online bit, read mode, build intent, read pressure, fill safety input, pressure fault fast path, initialize command, run test/user branch, heartbeat and delay.
- Test-mode priority is preserved: MENU, DOWN, ENTER, UP, DMX intent, safe off.
- `git diff --check -- app/task_control/task_control.c` reports no whitespace errors, only the existing LF/CRLF warning.
- Old naming/path scan found no `app_core`, `hal_if`, `domain/`, or accidental `app/rules_selftests` residue in current project source.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 21: ownership convergence (`fault` by safety, `state` by control)

Scope:
- In `app/task_control/task_control.c`:
  - removed direct fault latching/clearing in control path and startup selftest path.
  - `task_control_fill_safety_input()` now reads `app->faults.latched_mask` directly (no EventGroup reverse mapping).
  - `task_control_set_mode_state()` now only performs state transition.
- In `app/task_safety/task_safety.c`:
  - removed safety-side state transitions in `task_safety_handle_fault_state_and_dmx()`.
  - safety keeps fault ownership and safe-off enforcement.

Review:
- PASS with residual compile risk.
- Behavior-preserving points:
  - `SAFE_OFF` emergency command path remains active on fatal/E4/DMX-offline cases.
  - state transition is still centralized through control-side `task_control_transition()`.
  - fault bit publication remains in safety loop (`task_safety_update_fault_bits_and_log()`).

Residual risk:
- Not yet compiled with Keil in this environment.
- Startup selftest no longer latches local E1/E2 faults in control; it relies on safety loop to latch sensor/tilt faults.

## Step 22: parameter write path via `app_fsm` API

Scope:
- Added in `app/app_fsm.h` and `app/app_fsm.c`:
  - `app_fsm_apply_params()`
  - `app_fsm_get_params_snapshot()`
- In `app/task_ui/task_ui.c`:
  - replaced direct `app->params` in save handlers with snapshot+apply flow.
  - added `task_ui_apply_params_update()` and per-field mutators.
  - removed `commit_params` dependency from task config usage path.
- In `app/task_ui/task_ui.h`:
  - removed `commit_params` callback field from `app_task_ui_cfg_t`.
- In `project/src/freertos_app.c`:
  - removed legacy `app_params_commit()` binding and kept pure task wiring.
  - rebuilt file as startup/assembly entry after accidental truncation during charset conversion.

Review:
- PASS with residual compile risk.
- Behavior-preserving points:
  - UI setting save semantics remain field-by-field.
  - sanitize+save now executes inside `app_fsm_apply_params()`.
  - startup sequence remains: init -> load params -> rules selftest -> runtime objects -> task config -> selftest state -> watchdog -> tasks -> scheduler.

Residual risk:
- Not yet compiled with Keil in this environment.
- `freertos_app.c` was reconstructed to recover from encoding-side truncation; requires full compile validation.

## Step 23: legacy fault-bridge API deprecation marker

Scope:
- In `app/app_task_common.h`:
  - added `APP_TASK_DEPRECATED` macro for GCC/Clang toolchains.
  - marked `app_task_read_fault_mask_from_events()` as transitional/deprecated.
- In `app/app_task_common.c`:
  - added compatibility note on the function body.

Review:
- PASS.
- No behavior changes; only API-intent tightening to prevent new misuse.
- Current source scan shows no active call sites in app/runtime paths.

Residual risk:
- Legacy external callers (if any outside current repo scope) may see deprecation warnings.

## Step 24: AI-driven regression checklist landing

Scope:
- Added:
  - `doc/regression_checklist_ai_driven.md`
- Checklist covers requested scenarios:
  - DMX offline
  - E1/E3/E5
  - USER/TEST mode switch
  - SAFE_OFF preemption
  - UI parameter save path
- Updated `README.md` key-docs section with regression checklist link.

Review:
- PASS.
- Documentation-only step; no runtime behavior changes.

Residual risk:
- Checklist execution still requires on-target run and RTT log verification.

## Step 19: `freertos_app` task implementation split (`actuator` / `diag`)

Scope:
- Added new task modules:
  - `app/task_actuator/task_actuator.h`
  - `app/task_actuator/task_actuator.c`
  - `app/task_diag/task_diag.h`
  - `app/task_diag/task_diag.c`
- In `project/src/freertos_app.c`:
  - removed in-file implementations of `actuator_task()` and `diag_task()`
  - kept only startup/resource assembly responsibilities
  - added task config injection for the two new modules in `app_init_task_configs()`
- Updated Keil project file:
  - added `task_actuator.c` and `task_diag.c` into `project/MDK_V5/Flamethrower.uvprojx`

Review:
- PASS with residual compile risk.
- Behavior-preserving checks:
  - actuator command handling sequence remains unchanged (`SAFE_OFF/RELIEF/PUMP_ONLY/FIRE`)
  - fire timing logic (`igniter_delay_ms`, `oil_lock_delay_ms`, `fire_duration_ms`) unchanged
  - diag heartbeat watchdog logic unchanged (`EVT_HB_MASK`, 1000 ms miss timeout, 200 ms loop)
  - `SAFE_OFF` high-priority injection path remains unchanged in diag timeout case
- Structural gain:
  - `freertos_app.c` now moves closer to pure assembly/startup file
  - task behavior is localized in dedicated `app/task_*` modules

Residual risk:
- Not yet compiled with Keil in this environment.
- `project/src/freertos_app.c` remains in local non-UTF8 charset; future edits should continue preserving file encoding.

## Step 20: selftest extraction to `app_selftest`

Scope:
- Added new selftest module:
  - `app/app_selftest.h`
  - `app/app_selftest.c`
- Moved rule selftests out of `project/src/freertos_app.c`:
  - `app_selftest_state_machine()`
  - `app_selftest_dmx_strategy()`
  - `app_selftest_fault_manager()`
  - `app_selftest_safety_guard()`
  - `app_selftest_easy_dmx()`
  - `app_run_rules_selftests()`
- Updated `freertos_app.c` to consume `app_run_rules_selftests()` from the new module.
- Updated Keil project file:
  - added `app_selftest.c` and `app_selftest.h` in `project/MDK_V5/Flamethrower.uvprojx` (`app` group).

Review:
- PASS with residual compile risk.
- Behavior-preserving checks:
  - selftest set and execution order remain unchanged.
  - aggregate selftest log and per-item fail log remain unchanged.
  - startup flow still calls selftest once in `wk_freertos_init()` after params load.
- Structural gain:
  - `freertos_app.c` further converges to startup/assembly responsibility.
  - domain-rule selftest logic is now isolated in app-layer module.

Residual risk:
- Not yet compiled with Keil in this environment.
- `freertos_app.c` contains legacy encoded comments; this step preserved code behavior but did not normalize text encoding/comments.

## Step 16: rules selftest runner table-driven

Scope:
- In `project/src/freertos_app.c`, refactored `app_run_rules_selftests()` to use a local selftest table:
  - `selftest_item_t { name, fn }`
  - loop execution into `results[]`
  - existing aggregate selftest log retained
  - added per-item failure log: `APP_LOGW("selftest fail: %s", name)`

Review:
- PASS with residual compile risk.
- Behavior-preserving points:
  - all five selftests are still executed once per init cycle
  - aggregate summary log format and ordering remain unchanged
  - no new source files were added; Keil file list is unchanged
- `git diff --check -- project/src/freertos_app.c` reports no whitespace errors (only existing LF/CRLF warning).

Residual risk:
- Not yet compiled with Keil in this environment.
- `project/src/freertos_app.c` is edited with local default encoding to preserve existing file charset.

## Step 17: `task_safety` timeout fault chain split

Scope:
- In `app/task_safety/task_safety.c`, extracted timeout-fault rule chain helpers:
  - `task_safety_update_timeout_fault()` (shared timeout latch/clear flow)
  - `task_safety_check_fault_e1()`
  - `task_safety_check_fault_e3()`
  - `task_safety_check_fault_e5()`
- Main `safety_task` loop now calls these helpers directly for E1/E3/E5.
- E2 and E4 branches remain inline (simple non-timeout rules).

Review:
- PASS with residual compile risk.
- Behavior-preserving checks:
  - E1 still keeps immediate sensor-fault latch and warning log.
  - E1/E3/E5 timeout thresholds and clear conditions remain unchanged.
  - Fault publish/log, fatal/E4 state handling, DMX offline safe-off, and heartbeat order remain unchanged.
- No new source file was added, so Keil file list does not need update in this step.
- `rg` check confirms no legacy `app_core` symbol in `task_safety.c`.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 18: `task_safety` sample/fault phase split

Scope:
- In `app/task_safety/task_safety.c`, introduced `task_safety_sample_t` for one-loop runtime snapshot.
- Extracted:
  - `task_safety_collect_sample()`
  - `task_safety_check_fault_e2()`
  - `task_safety_check_fault_e4()`
  - `task_safety_run_fault_checks()`
- `safety_task` main loop now uses staged flow:
  - collect sample
  - run E1~E5 checks
  - publish fault bits and log
  - handle fault-state/DMX path
  - heartbeat delay

Review:
- PASS with residual compile risk.
- Behavior-preserving checks:
  - sensor and queue reads are still once-per-loop and in the same phase.
  - E2/E4 rules are unchanged, only moved into dedicated helpers.
  - fatal/E4 transition and DMX offline safe-off still run after fault checks.
- No new source file was added, so Keil file list does not need update in this step.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 15: `task_ui_setup_once` stage split

Scope:
- In `app/task_ui/task_ui.c`, split setup pipeline into focused helpers:
  - `task_ui_init_draft_params()`
  - `task_ui_bind_page_refs()`
  - `task_ui_bind_setting_handlers()`
  - `task_ui_log_initial_button_state()`
  - `task_ui_init_buttons()`
  - `task_ui_init_display_and_pages()`
- `task_ui_setup_once()` now orchestrates these stages with unchanged order.

Review:
- PASS with residual compile risk.
- Initialization order is preserved:
  - draft params
  - page refs
  - setting handlers
  - SlateUI port init
  - button-state log
  - button init/attach/start
  - display/page init
  - mark initialized
- No new source file was added, so Keil file list does not need update.
- `git diff --check -- app/task_ui/task_ui.c` reports no whitespace errors.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 14: `task_control` mode-branch extraction

Scope:
- In `app/task_control/task_control.c`, extracted two branch executors:
  - `task_control_run_test_mode()`
  - `task_control_run_user_mode()`
- `control_task()` main loop now keeps only high-level flow:
  - sample/update
  - pressure fault fast path
  - init command
  - dispatch to test/user mode handler
  - heartbeat tail (user mode path)
- No new files were added.

Review:
- PASS with residual compile risk.
- Behavior-preserving checks:
  - test mode still sends heartbeat in all exit paths
  - user mode still resets test-action sentinel each loop
  - fire path order is unchanged (`queue FIRE` then transition to `MACHINE_FIRING`)
  - pressure refill hysteresis thresholds are unchanged
- `git diff --check -- app/task_control/task_control.c` reports no whitespace errors.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 13: `ui_services` duplicate-path cleanup

Scope:
- In `app/ui_services.c`, extracted:
  - `ui_service_snapshot_changed()` for snapshot diff logic
  - `ui_service_call_setting_handler()` for nullable callback dispatch
- Replaced duplicated inline logic in:
  - `ui_service_set_machine_snapshot()`
  - all `ui_service_save_*()` adapter functions

Review:
- PASS with residual compile risk.
- Behavior unchanged:
  - snapshot validity and change detection criteria are the same
  - each save adapter still performs nullable callback dispatch with identical arguments
- No new files were added, so Keil file list does not need update in this step.
- `git diff --check -- app/ui_services.c` reports no whitespace errors.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 12: `task_ui` loop phase split

Scope:
- In `app/task_ui/task_ui.c`, introduced `ui_runtime_sample_t`.
- Extracted UI loop phases:
  - `task_ui_collect_runtime()`
  - `task_ui_update_snapshot_and_redraw()`
  - `task_ui_run_slate_once()`
- Kept existing `menu_active` update and heartbeat helpers from Step 11.

Review:
- PASS with residual compile risk.
- Loop order remains unchanged:
  - key ticks
  - runtime sampling
  - snapshot update + conditional redraw
  - SlateUI run
  - menu active update
  - heartbeat + delay
- No new source files were added, so Keil file list does not need update in this step.
- `git diff --check -- app/task_ui/task_ui.c` reports no whitespace errors.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 11: `task_ui` tiny split

Scope:
- In `app/task_ui/task_ui.c`, extracted:
  - `task_ui_is_menu_page()` for page-name classification
  - `task_ui_update_menu_active_flag()` for `menu_active` update
  - `task_ui_heartbeat_delay()` for heartbeat publish and loop delay
- Replaced inline menu-active expression and heartbeat tail in `ui_task`.

Review:
- PASS with residual compile risk.
- UI main-loop order remains unchanged:
  - key ticks
  - read system state
  - update UI snapshot
  - run SlateUI
  - update menu active flag
  - heartbeat and delay
- `git diff --check -- app/task_ui/task_ui.c` reports no whitespace errors.
- No unused helper remains in this file.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 10: `task_dmx` tiny split

Scope:
- In `app/task_dmx/task_dmx.c`, extracted:
  - `task_dmx_drain_hal_events()` for HAL byte polling and `edmx_rx_push_event` loop
  - `task_dmx_publish_heartbeat()` for event-group heartbeat publish
- Replaced inline code in `dmx_task` with helper calls.

Review:
- PASS with residual compile risk.
- Behavior and timing remain unchanged:
  - if config invalid: delay 10 ms and continue
  - drain all available DMX bytes
  - publish heartbeat bit (when event group is configured)
  - delay 1 ms
- `git diff --check -- app/task_dmx/task_dmx.c` reports no whitespace errors.
- No unused helper remains in file.

Residual risk:
- Not yet compiled with Keil in this environment.

## Step 8: startup runtime object and selftest-entry split

Scope:
- Extracted queue/event-group/DMX RX object creation from `wk_freertos_init()` into `app_init_runtime_objects()`.
- Extracted initial FSM transition to selftest into `app_enter_selftest_state()`.
- Kept `wk_freertos_init()` call order unchanged.

Review:
- PASS with residual compile risk.
- Boot sequence remains:
  - `ui_perf_init`
  - `app_fsm_init` + load params + rules selftests
  - runtime object creation
  - task config injection
  - enter selftest state and publish state bits
  - watchdog init
  - create tasks
  - start scheduler
- `git diff --check -- project/src/freertos_app.c` reports no whitespace errors.
- No naming-regression hits in source (`app_core`, `hal_if`, `domain/`, `app_run_app` all remain absent).

Residual risk:
- Not yet compiled with Keil in this environment.
- `project/src/freertos_app.c` uses non-UTF8 encoding; future edits should keep file encoding unchanged.

## Step 9: `task_safety` light helper extraction

Scope:
- In `app/task_safety/task_safety.c`, extracted low-risk repeated actions:
  - `task_safety_transition()` for `app_fsm_transition + app_task_set_state_bits`
  - `task_safety_update_fault_bits_and_log()` for fault-bit publish + mask-change log
  - `task_safety_handle_fault_state_and_dmx()` for fatal/E4 transition path and DMX offline safe-off path
  - `task_safety_heartbeat_delay()` for heartbeat bit + loop delay
- Replaced call sites in `safety_task` without changing E1~E5 fault condition logic.

Review:
- PASS with residual compile risk.
- Behavior order remains unchanged:
  - evaluate E1~E5 fault logic
  - publish fault bits and log deltas
  - handle fatal/E4 state transition path
  - DMX offline safe-off path
  - heartbeat and delay
- `git diff --check -- app/task_safety/task_safety.c` reports no whitespace errors.
- No new unused helper function remains in this file.

Residual risk:
- Not yet compiled with Keil in this environment.
- The user-mode branch is still intentionally coupled to `app_fsm`; this is acceptable for now because `app_fsm` is the application scheduler/state-machine object.
- `control_task` still owns `pressure_refill_active` and `last_test_action` as cross-loop state. Step 6 collects them into a small runtime struct.

## Step 6: `control_task` runtime and transition cleanup

Scope:
- Added `task_control_runtime_t` for loop-persistent state:
  - `last_test_action`
  - `pressure_refill_active`
- Added `task_control_transition()` for the common pair:
  - `app_fsm_transition(...)`
  - `app_task_set_state_bits(...)`
- Applied `task_control_transition()` in the main control loop and pressure-fault fast path.

Review:
- PASS with residual compile risk.
- Runtime state initial values are unchanged:
  - `last_test_action = (test_action_t)0xFF`
  - `pressure_refill_active = true`
- Reset behavior is unchanged: entering user mode still resets the last test action sentinel.
- Transition ordering is preserved in user-mode branches, including the fire branch where actuator command is queued before switching to `MACHINE_FIRING`.
- `git diff --check -- app/task_control/task_control.c` reports no whitespace errors, only the existing LF/CRLF warning.

Residual risk:
- Not yet compiled with Keil in this environment.
- Startup selftest still uses direct `app_fsm_transition()` calls because those branches are return-fast paths and were left untouched to keep this step small.

## Step 7: startup task-config split

Scope:
- Extracted task dependency wiring from `wk_freertos_init()` into `app_init_task_configs()`.
- Kept queue/event-group/DMX object creation in `wk_freertos_init()` so the boot sequence still shows object lifetime clearly.
- Kept task creation in `freertos_task_create()`.

Review:
- PASS with residual compile risk.
- Initialization order is preserved:
  - app init and params load
  - rules selftests
  - queue/event group/DMX rx creation
  - task config injection
  - app state switch to selftest
  - watchdog init
  - task creation
  - scheduler start
- Added no new ownership; the helper only assigns the same globals into the same task config structs.
- `app_params_commit()` already has a file-local forward declaration, so `ui_cfg.commit_params = app_params_commit` remains valid from the new helper location.
- `git diff --check -- project/src/freertos_app.c` reports no whitespace errors, only the existing LF/CRLF warning.

Residual risk:
- Not yet compiled with Keil in this environment.

