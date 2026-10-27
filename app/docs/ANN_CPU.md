# F28P55：恢复原CPU浮点ANN（2026-10-22）

当前RAM/FLASH使用原F28377D三种阻抗模型和一个共享J/D模型，全部为CPU float32网络：

1. `ANN_ConstV_Predict(Q, angle, &L, &R)`，Q为var、虚拟功角为度，输出L(mH)、R(Ω)。
2. ConstQ：`ANN_ConstQ_Predict(angle, V, &L, &R)`；Droop：`ANN_Droop_Predict(angle, V, &L, &R)`。angle为度，V为无功环电压修正量，来自`VSG_CTRL.v_delta_pu * SYS_V_BASE_V`，单位V；不是输出总幅值或PCC电压。保留旧工程对应归一化常数和权重。
3. 三种模式共用`VSG_JD_ANN2_Predict(R, L / 1000.0f, &J, &D)`，输出原标幺J=m_pu=2H、D=dp_pu。按并网时锁存的`REACTIVE_CTRL.active_mode`选择阻抗模型，不复制J/D网络。

权重、偏置、归一化及激活函数未改；不经过NPU、不量化，不裁剪Q或角度，不因超出历史训练范围拒绝计算。
虚拟功角仍使用原始omega_m积分与±0.015rad/s死区，不叠加PCC相角增量或锁存基准；见[VIRTUAL_ANGLE.md](VIRTUAL_ANGLE.md)。ConstV输入Q(var)/角度(deg)，ConstQ/Droop输入角度(deg)/电压修正(V)。所有特征在同一100点窗口平均，后台推理；模式/会话改变时取消旧结果，避免错用模型。
不再因L超过10mH或R超过0.97Ω拒绝结果。归一化公式仍然存在，它是网络数学的一部分，不是输入限幅。

`ANN_CTRL.input_in_range`、`impedance_in_range`仅指示是否处于历史训练范围，值为0不妨碍result_valid=1或J/D应用。
保留的基础数值检查：输入/结果不能是NaN/Inf，R/L不能为负，J/D必须有限且为正；应用时仍检查原欧拉离散条件。
这些不是训练域限制。每次推理都更新R_est/L_est/J_apt/D_apt，并通过CAN发送原始结果，包括负值、NaN和Inf；不再冻结为上次有效值。
无效结果仍清result_valid并报告原因，禁止写入控制器；预测无效不等于CAN超时。
若R/L非有限，后级J/D网络不执行，J/D发布NaN，表示本次没有可计算的结果。
允许外推不代表外推准确，平台上仍需对照真实R/L与响应验证。

## 构建与操作

- SysConfig已删除VSG_NPU实例，其他ADC/PWM配置不变；无需手动重配ADC。
- CCS四个配置均排除ann_npu.c；Flash已移除npu_model/mod.a。旧NPU文件保留归档，不参与编译链接。
- CCS刷新/重载工程配置后选择CPU1_LAUNCHXL_FLASH，Clean/Rebuild并下载，不能继续使用旧out或旧生成的board.obj。
- 已导出程序：artifacts/CPU1_LAUNCHXL_FLASH/VSG_BPNN_Codex_28P55.out。
- 主循环继续调用Inverter_Background；100点平均窗口在控制ISR收集，完整CPU推理在后台执行，可被控制中断抢占。
- 启动、预同步、并网、自动Pref阶跃/GPIO12变工况、J/D定时应用方式不变。
- 0x201～0x204布局不变；0x204的NPU使能/故障位及NPU状态字节固定0。OutputTelemetry版继承RawPrediction原始值显示，并增加0x205电压/运行频率；旧NaNFix版会在收到NaN/Inf时保留旧显示值。
- ANN_NPU_CTRL不再是本固件符号，CCS移除这组监视项。

## 监视

看 `ANN_CTRL.model_mode`（0/1/2）、`ann_Q_input`、`ann_Angle_input`、`ann_V_input`、`R_est`、`L_est`、`J_apt`、`D_apt`、
`result_valid`、`result_status`、`inference_count`、`apply_count`。
状态码保持兼容：0未采集，1等待，2有效，3仅指非有限输入，4非物理阻抗，5无效J/D，6数值计算失败。
旧上位机对3/4仍使用“输入越界/阻抗越界”文案；有限输入超过训练范围不会再产生状态3。
`INV_PARAM.vsg.h=J_apt/2`、`dp_pu=D_apt`才是实际控制参数；预测值不等于已经应用值。
`has_valid_result`仅表示本会话初始化以来曾有有效结果，不表示当前数值有效或当前仍保存历史有效值。
用户测试开关`annJdApplyEnable`默认0，屏蔽所有ANN J/D赋值；不影响推理与原始结果发送，本次保持该设置。

验证入口：tests/run_host_tests.ps1 -AnnOnly，覆盖旧28377D黄金向量、域外输入正常推理与应用、无效原始值发布/禁止应用和旧会话取消。
ConstQ/Droop另有162组旧函数逐项对照向量，验证输入顺序、归一化、两层激活及输出反归一化。三种模式都支持固定A和自适应B，B仍在9s/24s尝试应用有效J/D，其他时序不改。需要同时更新DSP和WPF。
本次迁移保证旧网络数值等效，不保证与训练工况等效：现控制器保留并网电压锚和Droop入网偏置；应在HIL核对`ann_V_input`与旧训练特征的基准及实际R/L，必要时重新采集训练数据。
tests/run_npu_host_tests.ps1现仅作为兼容入口转调CPU ANN回归，不再声称测试当前固件的NPU。
主机测试/构建不能替代板上CPU推理耗时及实际辨识精度验证。

2026-10-26迁移验证：ANN专项（含162组ConstQ/Droop旧函数对照）、40秒A/B/无功环专项、CAN专项、WPF测试通过；20MHz/10MHz FLASH及20MHz RAM构建通过。完整旧主机回归未全部通过：TestVsgPowerUnits仍按H=2.5硬编码首步频率增量，当前默认H=0.3，得到1.6666665e-5而非2e-6；此次未改动该控制参数或旧测试断言。
