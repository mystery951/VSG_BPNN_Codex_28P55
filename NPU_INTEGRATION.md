# VSG ConstV NPU integration

> Archived as of 2026-10-22. Production firmware is CPU-only in both RAM and Flash.
> NPU sources/models are preserved but excluded from the build; there is no runtime backend switch.
> See [CPU ANN instructions](app/docs/ANN_CPU.md). The remainder describes the former NPU implementation, not current operation.

The Flash configurations now use the board-validated ReLU regression surrogate
for `[Q(var), virtual_angle(deg)] -> [L(mH), R(ohm)]`. The original float
`ANN_ConstV_Predict` remains available for explicit CPU comparison. The JD
network and the mH-to-H conversion before it are unchanged.

## Build and use

Reopen/refresh this project in CCS so external `.cproject` changes are loaded.
Use `CPU1_FLASH` for the existing custom board or `CPU1_LAUNCHXL_FLASH` for
the LaunchPad clock/debug configuration. Clean, rebuild and download.
RAM configurations deliberately retain the original CPU inference backend.

The existing application eligibility remains: `INV_CMD.ann_enable == 1`,
grid-connected state, positive power reference and a valid grid PLL.
Existing J/D automatic/manual application commands are unchanged.

Watch:

- `ANN_NPU_CTRL.available == 1` and `enabled == 1`: Flash NPU backend selected.
- `started`, `completed`: should increase while eligible input windows arrive.
- `busy`: set only while an inference is pending.
- `status`: 0 idle, 1 running, 2 completed, 3 input outside domain, 4 timeout,
  5 API/nonfinite-output failure.
- `fault`: latched for timeout/API failures. Restart the firmware to reset it;
  do not manually clear it while hardware may still use its input buffers.
- `Q`, `angle`, `quantized_input`, `normalized_output`, `L_mH`, `R_ohm`:
  actual NPU inputs and outputs.
- `ANN_CTRL.L_est`, `R_est`, `J_apt`, `D_apt`, `result_valid`, `apply_count`:
  accepted application results and subsequent parameter updates.
- `ANN_CTRL.result_status`, `has_valid_result`: current validity/rejection reason and
  whether any valid estimate has ever been obtained. Published R/L/J/D now hold
  the last complete valid estimate (or zero before the first one), never NaN.
  CAN 0x204 carries this validity separately; held values are not fresh predictions.

To compare the original CPU network, set `ANN_NPU_CTRL.enabled=0`.
An already-started inference is drained before the backend changes. CPU mode
does not use or overwrite NPU buffers. No automatic silent CPU fallback occurs.

## Scheduling and validity

The control ISR still accumulates 100-sample mean Q/angle input windows. The
main loop locks only the input/result mailbox, launches NPU work with interrupts
enabled and returns immediately. Subsequent background calls poll completion;
they do not spin waiting and do not initialize PIE globally. CAN background work
continues. A latest-wins pending mailbox cannot overwrite an active inference's
separate static input buffer.

Disabling the ANN / leaving grid-connected state invalidates the prior session;
an old-session completion is not published to the current session. Timeout is
100 ms measured by control ticks and derived from `CONTROL_TS` (currently
0.0001 s). On timeout the buffers are not reused because hardware might still
be active.

Q/angle normalization follows the original model. Float-space padding is zero.
Input quantization mirrors the ONNX expression `floor(x*64+0.5)` with int8
saturation, including negative inputs; casting toward zero would be different.
The two model outputs are standardized floats. Their saved training means and
standard deviations restore L(mH)/R(ohm) before the original JD network.

The existing input/impedance ranges, finite/positive J/D checks and Euler
damping gate are retained. A completed NPU inference does not by itself imply
`ANN_CTRL.result_valid == 1`: the surrogate can produce an invalid impedance,
especially for synthetic rectangular-domain inputs. No physical-value clipping
was added.

## Model and memory

`npu_model/mod.a` and its generated header are snapshots from the independently
validated QAT run. Three FC hardware functions are in the linked Flash image.
The six ConstV teacher weight/bias arrays were checked against the existing
F28P55 CPU model and match. Model QAT statistics/scales and library provenance
are recorded beside the library.

SysConfig now includes the NPU module. The Flash linker places `.rodata.tvm`
in FLASH_BANK1 and `.bss.noinit.tvm` in previously unused RAMGS3. Model support
functions are included from the unmodified TI SDK snapshot
`npu_model/f28p55x_npu_impl.inc`. The `npu_model` folder is excluded from source
enumeration to prevent accidentally linking the archive twice.

## Verification

- Original ANN/JD/migration/control integration tests: `tests/run_host_tests.ps1 -AnnOnly`.
- NPU quantization, 8 golden vectors, output restoration, async mailbox,
  cancellation, timeout/wrap and API failure: `tests/run_npu_host_tests.ps1`.
- Custom-board Flash: `tests/run_target_build.ps1 -Flash -ExportDirectory artifacts/npu_flash`.
- LaunchPad Flash: add `-LaunchPad`, export to `artifacts/npu_launchxl_flash`.
- LaunchPad RAM compatibility: `tests/run_target_build.ps1 -LaunchPad`.

The complete host suite currently fails at the pre-existing 49 Hz grid-switch
check (`Expected 49.000011, got 49.996708`). The same failure was reproduced
using the untouched pre-integration `ann_adaptive.c`. This integration does not
change that control behavior. The new integrated VSG image still requires
board validation; the earlier standalone model test does not establish this
closed-loop image's behavior.

Pre-edit backup: `backups/before_npu_integration_20261020-153248.zip`.
