# 模块化逆变器统一操作手册

虚拟功角当前默认模式2：原omega_m积分建立工况后自动锁存基准，再叠加PCC相角增量。INV_CMD.virtual_angle_mode=0保留原算法回退，1保留直接补偿；操作、监视及边界以[VIRTUAL_ANGLE.md](VIRTUAL_ANGLE.md)为准。VSG功率环和PWM运行角不变。
实时显示新增0x205：电容端相电压峰值和逆变器运行角频率，使用OutputTelemetry版上位机；非RMS、非PCC PLL频率，详见[CAN_PROTOCOL.md](CAN_PROTOCOL.md)。

## 最新：恢复CPU ANN（2026-10-22）

已移除NPU运行路径、SysConfig实例及Flash模型库链接，原NPU文件只归档保留。
两级原始浮点ANN及权重不变；有限Q/虚拟功角不因训练范围越界而拒绝预测，固定R/L训练上限也不再拦截。
input_in_range/impedance_in_range仅作诊断；保留NaN/Inf、负阻抗、非正J/D及原离散条件检查。
之前关于NPU输入范围拦截的段落属于历史说明，以[ANN_CPU.md](ANN_CPU.md)为准。

## ANN无效结果与CAN显示修复（2026-10-20）

ANN_CTRL使用result_status和has_valid_result；R_est/L_est/J_apt/D_apt现保存最新原始预测，包含负值和NaN/Inf，不再冻结历史有效值。
无效时清result_valid，通过0x204报告原因；首次推理前数值为0，不能当作有效辨识。has_valid_result仅表示曾获得有效结果。
原始无效值不能绕过result_valid用于J/D更新。自动Pref试验仍按原固定时序，不重新加入等待。
需要同时下载新Flash程序、使用新版上位机；协议及诊断见[CAN_PROTOCOL.md](CAN_PROTOCOL.md)。
新版RawPrediction上位机直接显示原始NaN/Inf，不误报为通信超时；数值有效性检查与控制器赋值保持隔离。

## PLL启动/限幅优化（2026-10-20）

为解决部分启动相位下频率持续卡45Hz、预同步幅值随之被拉低的问题，已做以下修改：

- 第一次检测到有效PCC，以及PCC恢复/PLL重新使能时，以 `atan2(beta, alpha)` 初始化PLL自身的两份相角。
  正序d轴低通状态也按测得幅值初始化，避免零初值造成虚假的幅值爬升。不改 `INV_CTRL.theta`，不重置双环PI。
- 保留45～55Hz范围与原10kHz环路系数，但将 `ylf[0/1]` 同步限制在 `45-fn .. 55-fn`，避免输出限幅而内部状态继续积累。
  原宏入口转调同一个函数，归一化及限幅行为保持一致。
- 新增只读 `gridPllLocked`：正序d为正且超过30V、滤波q小于幅值5%、频率不贴限幅，
  并且10ms频率平均值每拍变化小于0.02Hz，连续满足100ms后置1；条件失效立即清0。
  该频率平均只用于锁定判断，不额外延迟PLL相角积分。
- `gridPllValid` 仍仅表示输入存在；预同步与合闸准入现在要求 `gridPllValid=1 && gridPllLocked=1`。
  未锁定时保持当前VF电压给定、清预同步补偿/就绪，不停止离网PWM；锁定后再执行原200ms等待及调幅/调相。
  `PRESYNC_CTRL.v_grid_mag` 在未锁定时显示0，原始PLL估算仍可看 `SPLL_GRID.d_p_decoupl_lpf/q_p_decoupl_lpf`。

无需修改SysConfig。下载新Flash程序，先不发并网命令，重复测试离网→预同步；若目标110V RMS，
检查 `INV_CMD.Vd_ref` 与 `PRESYNC_CTRL.v_inv_mag` 接近155.56V峰值，PLL频率跟随平台实际频率、q轴接近0，
`gridPllLocked` 最终为1，之后 `PRESYNC_CTRL.sync_ready` 为1 / `INV_CTRL.state` 为3，才按原流程请求并网。
不要手改锁定标志。`v_track_k` 默认仍为0.001；如曾为诊断设成0，重新下载初始化后会恢复默认值。

验证：216组启动相位/49～51Hz扫描、180°相角突变、频率/幅值阶跃、PCC掉电恢复，
ADC码量化与±0.1V噪声、叠加10%负序及2%五次谐波的PLL锁定测试均通过。
49/50/51Hz理想PCC下完整预同步及内环状态保持测试通过；主机测试不代替真实平台波形与计算耗时验证。
PLL相关编译常数在 `app/system_config.h`；ANN/JD、自动Pref时序、PWM接线及CAN报文未改。

## 自动功率/阻抗试验（2026-10-20）

`INV_CMD.auto_test_enable` 默认1；实际并网后1秒自动Pref=1和ANN辨识，10/30秒应用J/D，
15/35秒Pref=1.1、20/40秒回1，25秒GPIO12=1、70秒回0。
无效预测时保留原J/D、时序照常继续，不再等待；自动试验期间相关命令由流程持有。具体操作、取消行为及监视见[VSG_AUTO_TEST.md](VSG_AUTO_TEST.md)。
以下GPIO12独立手动控制说明适用于自动试验未运行时；自动试验取消时会将GPIO12复原为0。

## GPIO12平台变工况（2026-10-20）

GPIO12已由用户在SysConfig配置为普通数字推挽输出，初始化低电平，不再复用EPWM7。
CCS连续运行时修改INV_CMD.grid_impedance_step：0输出低电平、保持平台原工况；1输出高电平、触发平台设置的电网阻抗变化。
该命令为保持型电平，不是单次脉冲；写回0恢复低电平。输出在下一控制拍更新，正常周期100us。
独立于逆变器启停/CAN并网命令，不自动触发并网或J/D应用；停机不自动清除此工况命令，复位/重新初始化默认清0。
代码只控制GPIO，不定义半实物平台内部阻抗具体变化量；恢复工况还取决于平台模型对低电平的逻辑。

## MCANB与Flash更新（2026-10-19）

已迁移旧28377D的0x100/0x200命令和0x201/0x202/0x203遥测，接线GPIO58=TX、GPIO33=RX。
推荐CPU1_LAUNCHXL_FLASH，默认仍停机；具体报文、软件急停、监视变量和下载操作见[CAN_PROTOCOL.md](CAN_PROTOCOL.md)。
通信初始化由main调用CanPort_Init，命令由控制拍开头处理，遥测由main非阻塞发送。
Flash编译改为-O2/relaxed，RAM链接额外允许RAMGS2/3存放代码；ADC标定、预同步固定补偿及VSG参数未改。

## ADC接线更新（2026-10-18）

采集与换算代码已按以下接线更新，控制层物理量名称保持不变：

| ADC引脚 | 原始码监视变量 | 控制层物理量 | 固定零偏索引 |
|---|---|---|---|
| A0 | adcA0Raw | Va_cap（ua_cap） | 0 |
| A2 | 不读取 | 损坏，保留空采 | — |
| A5 | adcA5Raw | Ia_inv | 2 |
| A12 | adcA12Raw | Ia_filter | 9 |
| B1 | adcB1Raw | PCC_ub | 4 |
| B8 | adcB8Raw | Ib_inv | 5 |
| B10 | adcB10Raw | Ib_filter | 10 |
| C4 | adcC4Raw | Vc_cap（uc_cap） | 6 |
| C8 | adcC8Raw | PCC_uc | 7 |
| C14 | adcC14Raw | Ic_inv | 8 |
| D11 | adcD11Raw | Ic_filter | 11 |
| D12 | adcD12Raw | P_filter（W） | 12 |
| E12 | adcE12Raw | Vb_cap（ub_cap） | 3 |
| E15 | adcE15Raw | PCC_ua | 1 |

A2、B0损坏，保留SysConfig空采，不读取或用于控制；D14旧原始码变量已移除。
保留用户当前SysConfig：A/B每组4路、C每组3路、D/E每组2路，按实际SOC排列读取。
D11/D12目前各配置6次转换，本代码仅使用各自前4次取平均；E15读取SOC1/3/5/7的平均值用于PCC_ua。
全部13路读取均为4次平均，D12不替代现有INV_FEEDBACK.Pe/Qe功率计算。
13路零偏已按最新监视截图更新，原12路索引顺序不变，新增索引12为D12功率零偏1858码。
P_filter = (adcD12Raw - adcZeroOffset[12]) * POWER_SCALE，单位W，不是标幺值。
POWER_SCALE按用户给定3.3/4096*(20/3)*(1/1000)*1000000计算，约5.37109375 W/码，全部使用float常量避免整数除法与double运算。
电压/电流比例系数未更改；P_filter仅作为独立实测功率，不自动替换VSG或神经网络的计算功率反馈。
更换ADC引脚后，应在平台零物理输入下重新核对零偏，再检查各路正负极性与量程，确认后再使能控制。

## 1. 重构后的原则

命令、整定、运行状态分开；各对象不再既作输入又存PI参数。
控制算法不直接操作GPIO，硬件输出模块不判断控制模式。
唯一运行相角为INV_CTRL.theta，VF/预同步/VSG都连续使用它，不再维护另一个VSG角度副本。
保留原硬件配置和已验证计算顺序，本次不是增设保护或重新设计控制策略。

操作只使用INV_CMD；INV_PARAM用于模型/整定。
INV_CTRL、INV_FEEDBACK、INNER_CTRL、PRESYNC_CTRL、VSG_CTRL、MODULATION_OUT均只读，不手动伪造状态和就绪标志。
当前为单逆变器实例；PI状态由所属模块维护。不要将同一模块实例同时用于两个控制对象。

## 2. 编译和首次启动

1. 停止平台功率实验，CCS刷新工程，Clean/Rebuild当前RAM配置，下载新out。
   因文件移到hardware、measurement、math目录，必须重新生成构建列表，不能继续下载旧out。
   构建应包含inverter.c、inverter_settings.c及各模块；tests仍排除于目标编译。
2. 确认编译选项-O2、--fp_mode=relaxed；CONTROL_TS=100 us、PWM_PERIOD=7500与当前SysConfig匹配。
   未修改c2000.syscfg，保留PWM1零点SOCA、PRD中断、下一零点加载以及原互补死区。
3. Inverter_Init必须在Board_init之后、开启控制中断之前调用，只用于初始化，不在实时运行中反复调用。
   初始化先关闭PWM/断路器，再恢复默认命令和参数。主程序已接好。
4. 默认停机：INV_CMD.run_enable=0，presync_request=0，grid_connect_cmd=0，pll_enable=1。
   GPIO21=0（平台断路器断开），GPIO48/47=1（缓冲器关闭）。

文件默认值只在inverter_settings.c修改；运行中可以在CCS修改INV_CMD和INV_PARAM。
手动改变参数后若重新下载，仍以源码默认值初始化，不会自动保存CCS里改过的值。

## 3. 全部操作入口：INV_CMD

| 字段 | 默认值 | 含义 |
|---|---|---|
| run_enable | 0 | 0总停机，1运行；所有模式共用，不再叫offgridEnable |
| presync_request | 0 | 1请求预同步；并网后或总停机自动清0 |
| grid_connect_cmd | 0 | 手动置1请求并网；并网后保持1；置0开闸返回VF |
| pll_enable | 1 | PCC PLL监视使能，0不进行锁相；不是VSG功率环开关 |
| ann_enable | 0 | 1允许并网正有功工况下收集特征及后台辨识；不自动修改H/D |
| jd_apply_cmd | 0 | 手动置1，把当前有效预测应用一次；下一控制拍自动清0，无效时不排队 |
| jd_adapt_enable | 0 | 并网后置1，每个新有效预测自动应用一次；离开并网自动清0 |
| grid_impedance_step | 0 | GPIO12平台工况保持电平：0低，1高；独立于逆变器启停 |
| f_ref | 50 Hz | VF基础频率；开闸时接续并网末拍实际频率 |
| Vd_ref | 155.56 V | 当前d轴相电压峰值给定；预同步/VSG会自动更新 |
| Vq_ref | 0 V | q轴电压给定，当前流程保持0 |
| p_ref | 0 pu | 并网有功给定：0.1=500 W，0.2=1000 W，1=5000 W |
| q_ref | 0 pu | 并网无功给定：0.1=500 var，1=5000 var |

Vd_ref沿用原工程的工作给定含义，不是一个永远不变的“用户设定缓存”：
预同步跟踪PCC幅值、VSG调压时会更新它，退出后保留最后值。
如需新的VF电压，退出预同步并确认不在并网模式，再直接设置INV_CMD.Vd_ref。
并网时不要用Vd_ref与VSG功率环抢写电压。

## 4. VF→预同步→手动并网

### VF

保持grid_connect_cmd=0、presync_request=0，确认Vdc/Lf/Cf和采样方向正确。
设置INV_CMD.run_enable=1。默认给定155.56 V峰值；低幅值试验可在启动前改INV_CMD.Vd_ref=20。
第一拍准备占空比，下一零点加载后第二拍释放PWM，GPIO48/47变低，GPIO21仍低。
INV_CTRL.state=1表示VF。检查三相电压电流和INV_FEEDBACK。

### 预同步

在运行中设INV_CMD.presync_request=1。平台PCC需有正相序交流输入。
等待PRESYNC_CTRL.sync_ready=1，并观察theta_err、freq_err_hz、volt_err趋近0。
INV_CTRL.state：2预同步，3允许手动并网。
INV_CTRL.grid_zero_cross_ok=1表示三相瞬时差压均小于阈值，不是只检查A相过零。
预同步只调频率补偿和电压幅值，不覆盖运行相角。

预同步会把Vd_ref从当前值跟踪到PCC相电压峰值，例如20 V会逐渐升到155.56 V。
PCC低于30 V、PLL关闭或失去锁定时，撤销同步补偿/就绪并保持当前电压给定，不停止离网PWM；PCC恢复时重新初始化PLL并等待锁定。

### 并网

先用INV_CMD.p_ref=0、INV_CMD.q_ref=0。确认state=3、sync_ready=1、三相差压和平台波形正常后：
```c
INV_CMD.grid_connect_cmd = 1;
```

当前拍再次验证准入条件。接受后state=4，GPIO21输出1（平台断路器闭合），切入VSG。
不满足时命令清0、INV_CTRL.grid_connect_rejected=1，不排队、不延迟自动合闸；
恢复条件后必须重新手动置1。成功接受或总停机时清拒绝标志。

切换会保留相角、电压给定和四个双环PI积分器：
VSG_CTRL.omega_sync锁存预同步末拍实际补偿；首拍不更新功率环，
之后INV_CTRL.omega=VSG_CTRL.omega0+omega_sync+omega_m。
PRESYNC_CTRL同步状态清零是正常的，最终补偿已转交给VSG_CTRL。
不要再次把PLL角度加到INV_CTRL.theta，预同步积累的相角已经包含在其中。

并网稳定后可手动测试p_ref=0.1（500 W）→0.2（1000 W）。
当前用户整定Kp_QV=Ki_QV=0，保持ConstV；不保证Q精确等于q_ref，仍需平台验证。
正Pe按原工程定义为电容节点向滤波/电网侧输送有功；电流方向错误会形成错误反馈。
若Ia_filter包含本地负载，功率测量就包含相应负载，不等同于纯并网支路功率。

### 退出与停机

- INV_CMD.grid_connect_cmd=0：开闸，连续返回VF，保持当前相角、电压和实际频率。
  INV_CMD.f_ref会接续该频率，例如可能从50变为49；不自动重启预同步或重合闸。
- INV_CMD.run_enable=0：开闸、关闭缓冲器、封锁PWM，PI/调制/运行相角复位。
  预同步和并网命令清0；p_ref/q_ref不覆盖，重启前自行设为所需值。
- 停机后重启仍从VF开始，重新预同步、重新手动并网。

state=4仅表示软件已发出合闸命令，不代表读取到真实接触器闭合反馈。

## 5. 整定入口：INV_PARAM

| 分组 | 默认值和单位 |
|---|---|
| Vdc | 400 V，总直流母线电压；是模型参数，不是ADC测量 |
| grid_diff_max | 7.778 V，三相瞬时差压上限 |
| inner.Lf / Cf | 0.002 H / 0.000012 F |
| inner.Kpu / Kiu | 0.7 / 0.5，原已验证电压环增益 |
| inner.Kpi / Kii | 1.5 / 0，原已验证电流环增益 |
| inner.U_UpLimit / I_UpLimit | 300 A总电流给定限幅 / 100 V电流PI输出限幅 |
| presync.kp / ki | 4 / 10，相角PI |
| presync.omega_limit | 12.5663706 rad/s，即±2 Hz |
| presync.v_track_k | 0.001，每控制拍幅值跟踪系数 |
| presync.theta_ok / freq_ok / volt_ok | 约5度 / 0.5 Hz / 10 V峰值 |
| presync.wait_samples / ready_samples | 2000 / 10，10 kHz下200 ms / 1 ms |
| vsg.h / dp_pu | 2.5 s / 200，保留用户本次迁移前整定；m_pu=2h |
| vsg.Kp_QV / Ki_QV | 0 / 0，保留当前ConstV设置；单位pu电压/pu无功 |
| vsg.omega_limit | 有功环频率偏移限幅±2 Hz |
| vsg.v_min / v_max | 0 / 250 V；实际还受0.475*Vdc限制 |
| vsg.power_loop_div | 1：10 kHz；10：1 kHz，双环和相角仍10 kHz |

固定周期、基准及固定数值限幅在system_config.h；实测ADC零偏与比例在measurement/adc_calibration.h。
当前真实功率Pe/Qe除以5000，电压电流不标幺化。无功PI调节量v_delta_pu乘155.56后才变成V。
有功方程：d(omega_m_pu)/dt=(p_ref-Pe_pu-dp_pu*omega_m_pu)/(2*h)。
现在只保留h，不再要求同时维护m_pu=2*h这份重复参数，在线改h会直接影响功率环。
频率补偿omega_sync在并网时锁存；电网频率后来变化时，VSG按有功阻尼下垂响应，不保证Pe仍精确等于给定。
调整参数应停机完成，不要把无效值/零母线/非正惯量写入参数；本版不增加参数故障状态机。

## 6. 只读监视与旧变量迁移

| 原名称 | 新名称 |
|---|---|
| offgridEnable | INV_CMD.run_enable |
| PRESYNC_CTRL.enable | INV_CMD.presync_request |
| INV_CTRL.grid_connect_cmd | INV_CMD.grid_connect_cmd |
| gridPllEnable | INV_CMD.pll_enable |
| f_ref / Vd_ref / Vq_ref | INV_CMD.f_ref / Vd_ref / Vq_ref |
| VSG_CTRL.p_ref / q_ref | INV_CMD.p_ref / q_ref |
| Vdc、Lf、Cf、Kpu等 | INV_PARAM.Vdc、INV_PARAM.inner.Lf/Cf/Kpu等 |
| Kp_QV / Ki_QV、VSG_CTRL.h等 | INV_PARAM.vsg.Kp_QV/Ki_QV/h等 |
| PRESYNC_CTRL.kp/ki/阈值/计数上限 | INV_PARAM.presync对应字段 |
| colse_loop_theta、VSG_CTRL.theta | INV_CTRL.theta（唯一运行相角） |
| omega | INV_CTRL.omega，rad/s；除以2π为实际Hz |
| Vd_cap、Id_inv等dq量 | INV_FEEDBACK.Vd_cap、Id_inv等 |
| Pe、Qe、Pe_pu、Qe_pu | INV_FEEDBACK.Pe、Qe、Pe_pu、Qe_pu |
| id_ref、iq_ref、ud_ref、uq_ref | INNER_CTRL对应字段 |
| VaRef、VbRef、VcRef | MODULATION_OUT对应字段 |
| da_upper/da_lower | MODULATION_OUT.duty[0].upper/lower；B索引1，C索引2 |
| VSG_CTRL.v_ref | INV_CMD.Vd_ref，单份工作电压给定 |
| VSG_CTRL.v_delta | VSG_CTRL.v_delta_pu*155.56，删除重复存储 |
| VSG_CTRL.m_pu | 2*INV_PARAM.vsg.h，删除重复参数 |
| pwmBufferEnabled | pwmOutputEnabled（软件输出状态），GPIO实测另查寄存器 |
| offgridRunCount / gridPllRunCount | 已移除；ISR是否运行看pwm1IntCount |
| Va_sync_diff/Vb_sync_diff/Vc_sync_diff | INV_CTRL.sync_diff[0..2] |
| grid_zero_cross_threshold | INV_PARAM.grid_diff_max，真实V |

ADC有效原始码、Va_cap/Ia_inv/PCC_ua等12路物理量及SPLL_GRID.fo保持原名。
移除A9/B0/C14/D11无用raw变量/读取，但没有改变硬件SOC或EOC配置。
PLL的grid_theta仍是当前采样时刻角度，不要拿已经推进一拍的SPLL_GRID.theta[0]直接作相位比较。
私有PI和DQ工作对象不再是公开接口；不要在其他模块通过extern修改它们。
旧名字没有生产代码兼容宏；请更新监视表达式和外部调用，不要同时维护两套命令。

## 7. 模块调用约定

正常板上调用Inverter_Init、每100 us的Inverter_Step和主循环Inverter_Background；中断注册、ACK仍由control_isr负责。
Inverter_ControlStep只用于已提供测量/PLL数据的数值测试，不是第二个要同时调用的实时入口。
单独复用：
- InnerLoop_Calc接收参数、只读DQ反馈、真实电压/频率/母线/周期；输出INNER_CTRL。
- Presync_Calc接收参数和PRESYNC_INPUT快照，返回电压给定；不读取命令或操作模式/GPIO。
- VSG_PowerLoop接收参数、pu给定/反馈和周期，返回真实V；不会操作预同步或断路器。
- InvState_Step只选择模式并应答命令，切换动作统一在inverter.c。
- Modulation_Update将dq归一化量与下一PWM零点相角转成调制波及六路占空比。
- PwmOutput_Update只接受使能、合闸布尔量和占空比；不知道VF/预同步/VSG。
所有模块目前服务一个逆变器实例，控制中断内不重入调用同一份PI状态。

后续CAN/串口模块应更新INV_CMD，不要直接改INV_CTRL.state或GPIO21；
当前没有新增通信协议或自动指令调度。

## 8. 硬件及验证边界

GPIO21=0断开/1闭合；GPIO48/47低有效使能。EPWM1/2=A，3/4=B，5/6=C，7始终封锁。
保留影子寄存器准备一拍、0%/100%AQ强制端点、互补死区和手动OST停机封锁。
12路固定零偏顺序、3.3 V量程系数均未改；不加入动态零偏校准或滤波。

tests/run_host_tests.ps1运行真实C模块、假寄存器及数值模型：
12路ADC换算、统一Init/Step全链、单份命令/默认值、无扰PI状态保留、模式准入、
49/50/51 Hz锁相/预同步/并网、PCC恢复、500→1000 W理想电抗并网模型、手动开闸和停机、LC回归。
重构前后原场景打印结果一致；模型不能代替当前真实双环、接触器、采样误差和ISR时序的HIL验证。
TI目标编译已验证CPU1_RAM与CPU1_LAUNCHXL_RAM；测试目录仍从目标编译排除。
尚未下载板卡。没有过流/过压/防孤岛、真实断路器反馈或正式并网标准认证；
并网后PCC失电不会自动开闸，不用于实际电网带电试验。

本次备份：backups/before_modular_20261015_200146.zip。
原分步文档及旧布局均保存在备份，可恢复；不要将备份里的旧.c解压进当前源码目录并一起编译。

## 9. 28377D浮点ANN迁移（2026-10-16）

### 9.1 原模型与虚拟功角

保持旧main正在使用的两级网络，各为2→30→20→2，隐藏层tansig、输出层线性：

`Q(var), 虚拟功角(deg) → ANN_ConstV_Predict → L(mH),R(ohm) → VSG_JD_ANN2_Predict(R,L/1000) → J_apt,D_apt`。

权重、偏置、输入输出mapminmax常量与旧app/BPNN/ann_constv.c一致，没有重训或NPU量化。
不同模型原先两种tansig浮点表达式也分别保留。旧模型部分归一化参数源于MATLAB截图，
本次保留原值；模型一致不等于已经用原始训练数据验证实际辨识精度。

虚拟功角复现旧定义：`virtual_angle_int += deadband(omega_m)*CONTROL_TS`，
其中|omega_m|≤0.01 rad/s时置0，atan2(sin,cos)包裹到[-pi,pi]，再换成度。
VSG_CTRL.virtual_angle_int/rad/deg只是辨识特征，不是另一套调制相角；
不积分omega0、omega_sync或PLL角度，也不把它再次加到INV_CTRL.theta。
并网且p_ref>0时每100 us更新；退出并网/停机/非正有功给定时复位，避免跨工况沿用历史值。
这里将旧代码p_ref<=0时可能保留的旧虚拟功角改为明确复位。
它不是直接测得的电容电压与PCC电压相位差；49/51 Hz、频率漂移或不同预同步初值下，
不保证能替代训练时功角，必须重新核对模型工况，不能用锁相正常推断辨识正常。

### 9.2 快慢任务与数据一致性

- PWM ISR：按原控制流程计算Q、虚拟功角，每100个有效控制样本形成10 ms平均输入。
- 主循环：Inverter_Background取走完整邮箱，执行两个浮点网络，再发布完整结果。
- 下一PWM控制拍：仅在命令允许时写入H、D，之后运行原VSG功率环。

只有ann_enable=1、state=4、p_ref>0、gridPllValid=1时收集输入。
其他状态清除窗口和有效标志；后台正在运行的旧会话结果会被丢弃。
只在输入快照/结果发布时短暂屏蔽中断，神经网络计算全过程可被控制ISR抢占；
邮箱未处理时保留最新完整窗口，不在ISR等待推理。连续推理能否达到100 Hz需板上测时。
停机保留最后预测数值用于查看，但result_valid=0，不能再应用。

### 9.3 参数量纲与最小有效性检查

JD网络内部保留旧固定50 Hz、5000 W训练基准：预测的物理J、D乘314.15926²/5000。
因此ANN_CTRL.J_apt已经是m_pu=2H，ANN_CTRL.D_apt已经是dp_pu。
应用时仅执行`INV_PARAM.vsg.h=ANN_CTRL.J_apt*0.5f`、`dp_pu=ANN_CTRL.D_apt`，
不重复功率换算，不把训练频率改成PLL实时频率。

原始预测不裁剪：
- input_in_range：Q∈[-9746.9,969.2444] var，角度∈[2.6946,26.2217] deg。
- impedance_in_range：L∈[1,10] mH，R∈[0.1,0.97] ohm。
- result_valid：输入和R/L有限、R/L非负，且J/D有限且正；历史训练范围不作为拒绝条件，也不表示辨识置信度。
- 应用时还要求`D*CONTROL_TS*power_loop_div < 2*J`，避免该阻尼项前向欧拉离散失稳。
  这不是整个逆变器闭环的稳定性证明，也不替代合适的参数整定。

旧模型即便在输入矩形范围内，也可能预测负J/D或超范围R/L；现在发布每次原始预测，
通过result_status报告无效原因，拒绝写入控制参数，不触发停机、不增加故障状态机。观察apply_count是否增加来确认实际应用。
取消自适应不会自动恢复初始H/D，保持最后应用值；需要恢复时在停机后手动设置参数。

### 9.4 半实物操作顺序

1. 刷新工程、Clean/Rebuild、下载。确认新增adaptive目录的两个.c参与编译；tests仍排除。
2. ann_enable=jd_adapt_enable=jd_apply_cmd=0，先验证原VF→预同步→手动并网流程。
3. 在已验证的训练工况下建立正有功输出，保持ConstV；不要为获得有效标志随意扩大范围。
4. 设置INV_CMD.ann_enable=1，jd_adapt_enable保持0，仅观察，不改变H/D。
5. 观察VSG_CTRL.virtual_angle_deg、ANN_CTRL.ann_Q_input/ann_Angle_input、
   ANN_CTRL.L_est/R_est/J_apt/D_apt、result_valid、inference_count。
   L_est单位mH，R_est单位ohm；J_apt/D_apt是归一化参数。
6. 对照平台真实R/L和预期J/D确认正确且result_valid=1后，设INV_CMD.jd_apply_cmd=1。
   命令下一控制拍自动清0；apply_count增加1才表示实际应用，检查h=J_apt/2及dp_pu=D_apt。
7. 单次应用动态验证通过后，才可设INV_CMD.jd_adapt_enable=1，按每个新有效结果更新参数。
8. jd_adapt_enable=0停止自动应用；ann_enable=0停止辨识，不改当前H/D。开闸/停机后自动应用关闭。

原训练ConstV条件、固定有功工作点及频率范围仍需要核实；仅有Q与虚拟功角两个特征，
不能宣称覆盖任意负载、有功、电压、电网频率或拓扑。旧ConstQ/Droop/RLS没有混入这条链路。
NPU、在线权重训练、CAN新协议、旧10s/30s定时阶跃均不在本次迁移范围。

### 9.5 验证与文件

app/adaptive/ann_weights.h只存冻结参数，ann_models.c封装两级数学，ann_adaptive.c管理快慢数据和应用。
RAM链接为独立.ann_weights段分配RAMGS1，Flash链接放FLASH_BANK0；不修改ADC/PWM的SysConfig。
tests/run_host_tests.ps1包含12组旧代码独立生成的两级黄金向量、7组JD向量、
虚拟功角、100点窗口、只辨识不应用、手动/自动应用、无效值、离散条件和跨会话取消测试，
并将已有ADC假寄存器同步为用户已经完成的4次平均采样，保留原控制场景回归。
tests/run_target_build.ps1（可加-LaunchPad）在临时目录生成SysConfig并全源码编译链接，
不下载板卡、不改CCS生成文件。编译通过不等于HIL辨识/自适应闭环已验证。

迁移前备份：backups/before_ann_migration_20261016_094438.zip（含app/tests/main/SysConfig/工程元数据）。
