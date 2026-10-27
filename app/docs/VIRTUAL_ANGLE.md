# 虚拟功角：回退原积分算法（2026-10-23）

当前生产主流程inverter.c直接调用VSG_VirtualAngleStep，不调用PCC补偿或自动基准函数。此次只回退辨识特征，不改变PLL、调制相角、固定预同步补偿omega_sync、ANN权重和40秒A/B试验。

## 实际算法

输入VSG_CTRL.omega_m（VSG有功环角频率偏差，rad/s），不是omega总值，也不直接使用PLL频率。
在-0.015～+0.015rad/s（含端点）将输入置0，否则保留原值。
virtual_angle_int += omega_m * CONTROL_TS；
virtual_angle_rad = atan2f(sinf(virtual_angle_int), cosf(virtual_angle_int))；
virtual_angle_deg = virtual_angle_rad * 57.29577951308232f。

原始积分单位rad、不限幅；仅报告角度包络到约[-pi,pi]。保持当前F28P55原Step的0.015阈值，不借本次回退重新整定阈值。Pref<=0时清零，待机/退出并网沿原VSG_Reset路径复位。ANN仍要求并网、PLL有效、Pref>0，特征为Q与角度，计算有效标志继续供ANN使用。

## 已取消

PCC相对角增量、PCC相位重锚、初始稳态窗口/基准锁存、切换偏置以及特征切换时取消ANN窗口的额外逻辑全部退出生产路径。原频率突变处理不再生效，持续频差会继续累积虚拟功角，这是此次回退的预期行为。

INV_CMD.virtual_angle_mode默认0，保留结构布局便于CCS监视兼容，不再是有效切换命令；写1/2也不会重新启用PCC补偿。
VSG_CTRL.virtual_angle_mode固定为0；virtual_angle_legacy_int/deg只是原积分输出的同值镜像。PCC/phase/offset/angle_base字段保留布局并在复位时清零，不用于计算。INV_PARAM.virtual_angle为预留配置，不影响当前算法。
旧VSG_VirtualAngleUpdate接口仅转调原Step，忽略PCC和mode参数；VSG_VirtualAngleEstablish仅返回false，不做基准锁存。这些兼容入口不被生产主流程调用。

## 查看

- VSG_CTRL.omega_m：进入死区判断前的输入。
- VSG_CTRL.virtual_angle_int：未包络原积分rad。
- VSG_CTRL.virtual_angle_rad / virtual_angle_deg：包络后的ANN特征。
- VSG_CTRL.virtual_angle_valid：有限性/使能状态。
- ANN_CTRL.ann_Angle_input：ANN窗口平均角度。
- 不应再观察angle_base.ready来判断ANN是否开始，它现在保持0。

回退前代码（含原PCC补偿与旧测试）已保存为非编译.txt备份：
backups/virtual_angle_before_rollback_dd13fc2201c24f3aa9c2de45c422683f/
没有整工程回滚，最新CAN、P_filter/1ms遥测、40秒A/B以及9/24秒J/D赋值均保留。上位机不需要因本次回退更新。

## 验证

tests/run_host_tests.ps1 -VirtualAngleOnly：原死区/积分/包络逐点比对、持续频差故意累计、预留mode/PCC不影响结果、生产控制链路与ANN窗口运行、停机清零。另运行ANN、A/B自动试验和CAN专项及TI FLASH构建。此为编译/模拟验证，不代替HIL实测。旧总回归H数值夹具差异仍按POWER_COMPARISON.md说明保留。
