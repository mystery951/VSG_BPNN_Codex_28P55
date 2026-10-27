# F28P55逆变器：模块化VF / 预同步 / 手动并网VSG

2026-10-27：D12功率在4次ADC平均、零偏/比例转换之后增加约50Hz一阶低通。`P_filter_raw`为滤波前功率(W)，`P_filter`为滤波后功率(W)，原1ms CAN功率曲线自动使用后者，无需修改上位机。10kHz采样下系数0.0309275737（1-exp(-2π·50·100us)），在adc_measurement.c中设置；采样周期改变时需重新换算。初始化清零，停机不冻结/不反复清零滤波器；时间常数约3.18ms。该滤波不改变内部dq计算的Pe/Qe及VSG反馈。

2026-10-26：三种无功模式按旧工程V输出增益换算到标幺电压输出：ConstV=0/0、ConstQ=0/(40/155.56)、Droop=(10/155.56)/0。DSP确认并在并网时锁定；三种模式均使用对应CPU阻抗网络、共享J/D网络并支持A/B，见[无功模式操作](docs/REACTIVE_CONTROL.md)。

2026-10-22：已恢复原CPU浮点ANN，RAM/Flash均不再调用或链接NPU；Q/虚拟功角的训练范围仅作诊断，不拦截推理或J/D应用。
最新说明见[CPU ANN操作说明](docs/ANN_CPU.md)。
虚拟功角已回退原始omega_m积分：保留±0.015rad/s死区，取消PCC增量/自动基准/频率突变补偿。virtual_angle_mode保留为兼容字段，默认0，不再切换算法；见[虚拟功角说明](docs/VIRTUAL_ANGLE.md)。
新增CAN 0x205：电容端实测相电压峰值(V)与逆变器运行角频率(Hz)，每100ms更新；搭配OutputTelemetry版上位机，见[CAN协议](docs/CAN_PROTOCOL.md)。
2026-10-23：新增按钮40秒A/B实验：并网后Pref=0等待，功率改用HIL P_filter；1ms 0x206/207、100ms实际J/D 0x208及实验应答0x209。A固定参数，B在9/24秒两次整定，见[功率实验对比与操作](docs/POWER_COMPARISON.md)。

本次是结构重构，不重新整定控制器。5000功率基准、真实V/A双环、固定ADC零偏、
PWM同步/死区、C相EPWM5/6和GPIO极性均保持。仅用于半实物，未补入硬件保护。

## 从哪里开始

- **日常手动输入：INV_CMD**。不要再操作offgridEnable、PRESYNC_CTRL.enable或VSG_CTRL.p_ref。
- **模型和整定：INV_PARAM**。全部默认值在inverter_settings.c，结构定义在inverter_settings.h。
- **状态/相角：INV_CTRL**，只读。整个系统只保留INV_CTRL.theta这一份运行相角。
- **按钮试验**：`INV_CMD.auto_test_enable` 默认1表示允许A/B按钮实验，不再并网自动开始；时序和操作见[自动试验说明](docs/VSG_AUTO_TEST.md)。
- **PLL监视**：`gridPllValid` 是输入存在，`gridPllLocked` 是连续稳定100ms的锁定判据；未锁定时不进行预同步调幅/调相。
- **完整操作、旧变量迁移表、数学和验证边界：[统一操作手册](docs/INVERTER_GUIDE.md)**。

## 文件分配

```text
app/
  inverter.h/.c               总入口和唯一调度器、模式切换动作
  inverter_settings.h/.c      INV_CMD、INV_PARAM及全部运行默认值
  system_config.h             固定周期、功率/电压基准、调制等编译常数
  adaptive/
    ann_models.h/.c          两级2-30-20-2浮点网络、原归一化和激活函数
    ann_weights.h            从28377D逐项复制的ConstV/ConstQ/Droop及共享JD权重
    ann_adaptive.h/.c        100点输入平均、后台邮箱、有效性和J/D应用
  control/
    inverter_state.h/.c       只选择运行模式、准入/拒绝合闸命令
    inner_loop.h/.c           四个PI、真实值前馈/解耦，不处理模式
    presync.h/.c              预同步PI和就绪判断；显式输入、返回电压给定
    vsg.h/.c                  标幺功率环；显式输入、返回真实电压给定
    vsg_auto_test.h/.c        并网后Pref阶跃、两次辨识/JD应用、GPIO12变阻抗时序
    three_level_modulation.*  反Park、三相调制限幅、三电平占空比
  measurement/
    adc_measurement.*         12路有效原始码及物理量换算
    adc_calibration.h         固定零偏/换算系数
    feedback_dq.*             dq反馈和P/Q，统一输出INV_FEEDBACK
  PLL/                       已验证的DDSRF算法及PCC锁相包装
  math/pi_controller.*       与策略、硬件无关的通用位置式PI
  hardware/pwm_output.*      只接收使能、合闸布尔量和六路占空比
  isr/control_isr.*          注册中断、调用Inverter_Step、清中断标志
tests/                       假寄存器和实际C模块主机回归，不参与目标编译
backups/                     重构前源码/工程配置压缩包
```

测量值如Va_cap、Ia_inv、PCC_ua和12个有效adc*Raw名称保留，方便对照原硬件通道。
A9/B0/C14/D11不参与控制，其无用读取/监视变量已删除；SysConfig中的SOC时序未改。
DQ变换的工作对象及PI积分器变成模块私有，不再作为操作命令暴露。
原offgridRunCount、gridPllRunCount、SBase副本、VF/VSG重复相角等已去掉。

## 接口与调用顺序

主程序只需在Board_init后、开启TBCLKSYNC/EINT前调用：
```c
#include "app/inverter.h"
Inverter_Init();
```

10 kHz的PWM1 ISR只调用：
```c
Inverter_Step();
```

主循环（中断开启）还需反复调用 `Inverter_Background();`，执行待处理的浮点ANN推理。
现有main已接入，并把500 ms阻塞闪灯替换为非阻塞计数；不能把后台函数放进PWM中断。
ANN操作入口是 `INV_CMD.ann_enable`、`jd_apply_cmd`、`jd_adapt_enable`，默认均为0；
虚拟功角看 `VSG_CTRL.virtual_angle_deg`，预测结果看 `ANN_CTRL`。
完整操作和模型适用边界见统一手册第9节。不是NPU或在线权重训练。

内部固定顺序：
采样换算 → PCC PLL → 当前相角dq/功率反馈 → 预同步 → 模式判断/切换 →
VSG功率环或VF参考 → 电压/电流PI → 推进唯一相角 → 调制 → PWM/断路器。
新相角用于下一PWM零点的影子寄存器加载，采样仍用当前拍相角。

Inverter_ControlStep是预采样后的数值测试入口，不读ADC、不更新PLL、不写GPIO；
正常板上运行请使用Inverter_Step，不要在ISR中同时调用两者。

## 对象职责

| 对象 | 性质 | 内容 |
|---|---|---|
| INV_CMD | 操作输入/运行给定 | 启停、预同步、手动并网、PLL使能、VF给定、P/Q给定 |
| INV_PARAM | 整定输入 | Vdc、双环、预同步、VSG参数及三相差压阈值 |
| INV_CTRL | 只读 | 状态、相角/角频率、差压和拒绝标志 |
| INV_FEEDBACK | 只读 | dq实际V/A、真实P/Q及5000基准pu功率 |
| INNER_CTRL | 只读 | id_ref/iq_ref、归一化ud_ref/uq_ref |
| PRESYNC_CTRL | 只读 | 同步误差、计数和频率补偿 |
| VSG_CTRL | 只读 | 已锁存的频率补偿、有功动态和无功电压调节量 |
| ANN_CTRL | 只读 | 平均Q/虚拟功角、R/L和J/D预测、有效标志及推理/应用次数 |
| MODULATION_OUT | 只读 | VaRef/VbRef/VcRef及A/B/C上下桥占空比 |

INV_CMD.Vd_ref是原有“当前运行电压给定”，预同步/VSG会更新它；
手动开闸后INV_CMD.f_ref接续开闸前实际频率。保留这些行为，避免本次重构改变已验证流程。

## 验证与恢复

- 重构前全部主机测试通过，已保存基准；迁移后同组测试打印结果一致。
- 新增完整Init/Step链路及不读取私有PI的切换积分保持测试。
- 当前SysConfig生成0错误（1条通用Device Support建议）。
- CPU1_RAM / CPU1_LAUNCHXL_RAM：TI 25.11.0.LTS、O2、FPU32/TMU1、relaxed全源码编译链接通过。
- 尚未下载板卡，不能用主机模型代替真实ISR耗时和HIL动态验证。
- 备份：../backups/before_modular_20261015_200146.zip。
- 原三份逐步操作文档已合并到统一手册，旧版可从备份恢复。

没有过流/过压/防孤岛和真实接触器反馈；不用于实际电网带电试验。

2026-10-16 ANN迁移：保留用户当前4次ADC平均、D=200、Kp_QV=Ki_QV=0及其余整定。
ANN权重独立放在`.ann_weights`段，RAM配置使用空闲RAMGS1，Flash配置放Flash；
不占用原控制数据RAMLS5。现已补齐ConstQ/Droop阻抗网络，保留共享JD网络；未迁移RLS。
迁移前备份：`backups/before_ann_migration_20261016_094438.zip`。
