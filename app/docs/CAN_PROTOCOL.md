# F28P550 MCANB 对接说明

2026-10-22：当前固件已切回CPU ANN，0x201～0x204布局不变，0x204字节2的NPU使能/故障位及字节3固定0。
Q/虚拟功角不再受训练范围拦截，R/L固定训练上限也不拦截；历史范围标志仅作CCS诊断。
状态3现仅表示非有限输入，4表示负阻抗；0x201/202始终发送最新原始预测，不再保留旧有效值。无效预测仍禁止写入控制参数。
新增0x205逆变器相电压峰值与运行频率，上位机使用OutputTelemetry版；原0x201～0x204布局不变。

## 硬件与构建

使用已确认的 MCANB：GPIO58=TX，GPIO33=RX（100PZ物理脚67和53）。
需外接CAN收发器；GPIO不能直接接CANH/CANL。总线接收发器与上位机适配器，正确终端匹配并连接参考地。
GPIO4/5继续用于PWM3，GPIO47/48继续用于缓冲器，GPIO21继续用于断路器。

SysConfig新增CANB_TX_RESERVED、CANB_RX_RESERVED两个GPIO实例，只作引脚占位/上电初态。
Board_init先将TX置高、RX输入；其后的CanPort_Init统一切换MCANB复用并配置MCAN。
GPIO33与ADC B24/D1/E1共享，CanPort_Init显式关闭该引脚模拟模式并使用异步输入，不能再用该脚作模拟采样。
不要另加第二个MCAN初始化程序或在运行中重新调用Board_init，否则会覆盖配置。
MCANB按150MHz SYSCLK/5得到30MHz位时钟，30TQ、80%采样点，经典CAN 1Mbps。
无内部回环、无FD、无BRS；上位机应使用正常主动模式，监听模式不提供ACK。

推荐CCS配置CPU1_LAUNCHXL_FLASH，与原LaunchPad 20MHz晶振一致。
保留CPU1_LAUNCHXL_RAM；不是LaunchPad硬件时，应核对晶振后选择CPU1_FLASH，不能盲目切换。
FLASH与RAM现均使用-O2、--fp_mode=relaxed；FLASH定义_FLASH、入口code_start=0x080000，SDK复制.TI.ramfunc后初始化Flash等待周期。
尚未自动烧录或改变板上启动引脚。断电独立启动需硬件选择Flash boot。
CCS刷新工程后选择对应FLASH配置，Clean/Rebuild并下载新out，不能继续下载旧RAM out。
本次导出的Flash二进制位于artifacts/CPU1_LAUNCHXL_FLASH/VSG_BPNN_Codex_28P55.out，map同目录。
可用tests/run_target_build.ps1 -LaunchPad -Flash -ExportDirectory artifacts/CPU1_LAUNCHXL_FLASH重新生成；这是构建/导出，不执行烧录。

## 旧28377D协议保持兼容

经典CAN、1Mbps、11位标准数据帧。多字节Intel/little-endian，float为IEEE754 float32。

| ID | 方向 | DLC | 数据 |
|---|---|---|---|
| 0x200 | PC→DSP | 4 | 命令 A5 5A 00 |
| 0x100 | PC→DSP | 4 | FF A5 5A 00（软件急停） |
| 0x201 | DSP→PC | 8 | R_est(float32,Ω)，L_est(float32,mH) |
| 0x202 | DSP→PC | 8 | J_apt(float32)，D_apt(float32) |
| 0x203 | DSP→PC | 8 | 状态、命令应答和计数，见下表 |
| 0x204 | DSP→PC | 8 | ANN结果有效性及原因，新增诊断帧 |
| 0x205 | DSP→PC | 8 | 电容端相电压峰值(float32,V)，运行角频率换算值(float32,Hz) |

遥测周期100ms，由10kHz控制计数驱动，在主循环发送，不在控制ISR中等待发送。
ANN上电默认关闭；并网03在任何auto_test_enable设置下都不自动启动ANN或赋Pref=1，等待05固定A/06自适应B。auto_test_enable默认1现在仅是允许按钮试验。
完整40秒时序见[VSG_AUTO_TEST.md](VSG_AUTO_TEST.md)。B在9/24秒尝试应用J/D，无效标记失败且不暂停时序；重复03不重置试验或Pref。运行/完成后重复05/06被拒绝，需急停/解除/重新并网后再开始下一组。
实际采集/预测仍要求已进入并网状态、PLL有效、有功给定为正。
0x201/202保持原float32布局，发送最新原始预测（允许负值、NaN/Inf），首次推理前为0；是否有效必须结合0x204。R/L非有限时不执行后级网络，J/D为NaN。
J_apt是旧模型的m_pu=2H，不能误读成当前H或物理转动惯量；报文发送预测值，不保证已经应用到控制。
自动试验在指定时间应用有效预测J/D，阶跃响应期间冻结参数；禁用自动试验后仍可使用原手动/自适应命令。
旧0x201～205布局不变；新增206/207传输Pref/P_filter，208实际J/D、209实验应答，见文末。OutputTelemetry版继承RawPrediction原始非有限值显示，旧NaNFix版可能保留旧显示值。

## 0x205输出电压与运行频率（100ms）

| 字节 | 信号与单位 |
|---|---|
| 0~3 | sqrt(Vd_cap²+Vq_cap²)，电容端实测相电压峰值，float32小端，V |
| 4~7 | INV_CTRL.omega/(2π)，逆变器运行角的频率，float32小端，Hz |

电压不是INV_CMD.Vd_ref，也不是线电压或有效值。平衡正弦时155.56V峰值对应约110V有效值。
频率不是SPLL_GRID.fo，不是PCC频率或独立ADC过零测频；反映逆变器控制运行角。停机时也可能为参考频率，是否实际输出PWM应结合0x203状态判断。
运行角频率并不证明实际端口电压波形有效。电压不平衡或谐波会使瞬时dq幅值有纹波，本次未新增基波/RMS滤波。
新增数据不依赖ANN使能或预测有效性；nan/inf照常发送，上位机标注数据无效，不误判为报文超时。
Vd/Vq/omega与原遥测在短临界区取一致快照，sqrt/换算在开中断后的主循环执行，不增加控制ISR计算。
MCANB增加独立slot4，TX buffer数量由4改为5，过滤器、RX buffer、波特率和引脚不变。
上位机在实时参数第三行显示“相电压峰值”和“频率（运行角）”，0x205超过500ms未更新时两项独立显示通信超时，不受ANN状态影响。
PC程序：D:/Project/VS2026/CANFD/07_Application/CANFD_Studio/ZLGCAN/artifacts/OutputTelemetry/ZLGCAN.exe，同目录DLL/config必须保留。

## 0x204预测状态（100ms）

| 字节 | 含义 |
|---|---|
| 0 | 格式版本1 |
| 1 | 0未采集/禁用，1等待首次结果，2当前结果有效，3输入非有限，4预测阻抗为负，5J/D无效，6推理失败 |
| 2 | bit0曾获得有效预测，bit1 ANN命令使能，bit2 NPU使能，bit3 NPU故障锁存 |
| 3 | 当前CPU版本固定0；历史NPU状态字节 |
| 4~7 | ANN_CTRL.inference_count，uint32小端，包含有效/无效结果的处理次数 |

0x204与0x201/202/203在同一临界区取快照，0x205也在此快照采集Vd/Vq/omega；五个独立TX buffer非阻塞提交。
无效推理仍发布原始R/L/J/D，result_valid清0，禁止应用到控制器。上位机按原始值显示并标注无效原因，不把收到NaN/Inf视为通信超时。
恢复有效输入后自动恢复有效状态，无须手动清除状态。当前固件无NPU运行路径。
诊断原因以ANN_CTRL.result_status为准；原始输出看ANN_CTRL.L_est/R_est/J_apt/D_apt，输入看ann_Q_input/ann_Angle_input。
上位机将“报文未到”“收到NaN/Inf”“诊断帧报告预测无效”分开显示，不把无效预测误报为CAN超时。
输入/输出训练范围及J/D应用条件未放宽，不对输入裁剪；本修复不表示模型能有效辨识训练域外的工况。

## 命令与操作顺序

| 操作 | ID | 4个数据字节 | 准入条件 |
|---|---|---|---|
| 离网启动 | 0x200 | 01 A5 5A 00 | 待机或离网VF |
| 开始预同步 | 0x200 | 02 A5 5A 00 | 离网VF、预同步或并网就绪 |
| 请求并网，默认随后执行自动试验 | 0x200 | 03 A5 5A 00 | 并网就绪且sync_ready=1；已并网可重复 |
| 软件急停 | 0x100 | FF A5 5A 00 | 所有状态 |
| 解除软件急停 | 0x200 | 04 A5 5A 00 | 急停已锁存，控制器已经停机 |

先用适当低电压给定验证；03并网后的默认试验含5000W→5500W阶跃，须确认平台容量、直流侧与负载允许，再进行功率实验。
上位机每发一步后观察0x203应答，再发下一步，不能同时高频连发全部命令。
普通命令采用旧工程的单槽“最新命令覆盖”规则；急停使用独立锁存，不会被后来的普通命令覆盖。
接收ISR只校验/递交，控制ISR在同一周期边界应用。正常启动仍有一拍PWM影子寄存器预装载。
03只产生并网请求，原状态机还会重新检查PLL、同步与相电压差，不能绕过准入直接闭合断路器。
急停下一控制拍清运行/预同步/并网请求，关闭缓冲器并打开断路器；每拍强制保持停机，手改run_enable也不能绕过。
04只解除CAN软件急停并保持待机，必须再次01启动，不会自动恢复或重合闸。
这是CAN软件停机，不替代硬件急停/Trip Zone；CAN断线不构成可靠安全停机链路。

## 0x203状态格式

| 字节 | 含义 |
|---|---|
| 0 | 状态：0待机，1离网VF，2预同步，3并网就绪，4已并网，5CAN急停锁存 |
| 1 | 最近已处理命令：1/2/3/4/255，初始0 |
| 2 | 结果：0无、1接受、2当前状态不允许、3急停锁存拒绝 |
| 3 | bit0 PWM释放，bit1断路器GPIO闭合命令，bit2同步就绪，bit3急停锁存，bit4曾见CAN错误，bit5运行使能，bit6预同步请求，bit7并网请求 |
| 4~5 | cana_rx_count低16位，小端 |
| 6 | 接受命令计数低8位 |
| 7 | 拒绝命令计数低8位 |

线上状态5由CAN急停锁存映射而来，内部INV_CTRL.state仍为安全待机0；不向原精简控制器添加通用故障状态机。
非法DLC、错误校验字节和未知命令不执行，计入invalid_count；不会修改最近执行应答。
非匹配ID、扩展帧和远程帧由硬件过滤，不保证产生接收计数；接口也会拒绝非标准数据帧/FD帧。
断路器状态是GPIO命令，不是辅助触点实际位置。

## CCS监视与首测

下载新Flash程序，连续运行，默认INV_CMD.run_enable=0、断路器打开、缓冲器禁用。
先不发送启动命令：检查cana_port_ready=1、cana_init_error=0，PC应每100ms收到五帧0x201～0x205。
发送01后cana_rx_count与accepted_count递增，再按02→等待就绪→03测试；最后分别验证FF停机和04解除。
ADC/PLL未就绪时不要强制伪造状态并网。

- cana_telemetry_enable：默认1，控制0x201/202/205；cana_status_enable：默认1，独立控制0x203/204。
- cana_status_tx_data[8]：最近0x203原始字节。
- cana_command_invalid_count：格式错误数；cana_command_result：当前命令结果。
- cana_tx_count：硬件成功发送（获得ACK）的累计帧数；cana_telemetry_cycle_count/cana_status_tx_count：已成功提交的组数/状态帧数，不代表已ACK。
- cana_output_tx_count：0x205正常约10次/秒；cana_telemetry_cycle_count仍只统计201/202配对。低速五帧加208/209/20A/20B共90帧/秒，再加功率206/207约2000帧/秒，总计约2090帧/秒（无错误重传时）。
- cana_tx_busy_count：发送缓冲仍占用时的跳过次数。无接收节点或适配器只监听时可能不增长tx_count并持续忙，控制ISR不阻塞。
- cana_error_flag：MCAN错误中断累计；cana_err_status：最近错误IR位掩码，不是旧DCAN ES寄存器。
- cana_isr_status：最近MCAN中断IR位掩码；cana_bus_off：总线关闭标志。
- cana_init_error：1 RAM初始化等待超时、2进入配置模式超时、3配置API失败、4进入正常模式超时。等待均有上限，不会无限卡在main初始化。

Bus-off不自动恢复并启动：修复波特率、接线和终端后停机重新复位下载。诊断变量保留cana_前缀用于兼容，实际外设是MCANB。

## 已验证与未验证

本地测试覆盖报文位序/单位、格式拒绝、命令准入、急停优先级与锁存解除、100ms节拍/回绕、ADC→控制→PWM接口。
目标构建使用本机C2000Ware 26.01与TI编译器25.11。
编译/模拟测试不能替代真实MCAN收发、波特率、总线ACK与Flash复位启动测试；未自动连接调试器或烧写硬件。

原理与API依据：本机C2000Ware f28p55x mcan_ex7_classic_transmit.c、mcan_ex10_receive_multiple_buffers.c、mcan.h/mcan.c；
[TI F28P55x技术参考手册](https://www.ti.com/lit/ug/spruj53/spruj53.pdf)。

## 2026-10-23功率实验扩展

0x206/207每1ms采样并带相同uint32控制tick（100us单位），分别跟随float32 Pref_W / P_filter_W（HIL D12）；0x208每100ms为float32实际采用J=2H / D。0x209每100ms为start_tick(u32)、run_id(u16)、mode(u8)、flags(u8)，确认真实DSP起点及两次参数赋值成功/失败。新增05/06 A5 5A 00按钮命令，均0x200 DLC4。原五种指令和0x201～205保持。
TX槽当前为11（新增20A/20B）；中断只采样，主循环非阻塞传输。新增高频重试会增加cana_tx_busy_count，它不是丢样计数；真实队列满丢样看cana_power_drop_count，采样数/提交帧对数看cana_power_sample_count/cana_power_tx_count。
详细协议、负载预算、1ms采样与PC到达时间区别及A/B操作见[POWER_COMPARISON](POWER_COMPARISON.md)。

## 2026-10-26无功模式扩展

0x200 DLC4增加10/11/12 A5 5A 00（命令字为十六进制）选择ConstV/ConstQ/Droop，只允许并网前且未发出合闸请求。并网后锁定。
0x20A DLC8每100ms：版本1、已选mode、生效mode(255未启用)、锁定bit0、接受版本号u16、结果0/1/2、最近请求mode。
0x20B DLC8每100ms：float32 Qe(var)、float32 Qref(var)，小端。原帧布局不变。详细增益/无扰/ANN边界见[REACTIVE_CONTROL](REACTIVE_CONTROL.md)。
