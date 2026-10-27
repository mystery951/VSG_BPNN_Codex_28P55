# 按钮触发40秒A/B实验（2026-10-23，替代原并网自动70秒流程）

并网不再启动试验、不设置Pref=1、不启用ANN。两个WPF按钮“固定参数启动·A”“自适应参数启动·B”通过CAN05/06启动；必须已实际并网、未进行过本轮实验、无急停。每次连接只允许一次，重复点击不重启计时，完成后需急停/解除/重新孤岛预同步并网。

## 时序（DSP真正接受启动命令并设置Pref=1的控制tick为0）

| 时间 | A/B共同动作 | B独有动作 |
| --- | --- | --- |
| 0 | Pref=1，GPIO12=0，开始ANN新窗口和40秒记录 | 允许本轮两次J/D应用 |
| 0～9s | 保持Pref=1，建立工况、进行预测 | 无 |
| 9s | A保持基准H/D | 尝试应用初始阻抗对应的新鲜有效J/D |
| 10s | Pref=1.1 | 无 |
| 15s | Pref=1 | 无 |
| 20s | GPIO12=1，HIL阻抗突变；清除旧ANN窗口、待发布结果 | 无 |
| 24s | A保持基准H/D | 尝试应用新阻抗对应的新鲜有效J/D |
| 25s | Pref=1.1 | 无 |
| 30s | Pref=1 | 无 |
| 40s | 完成记录，保持Pref=1、GPIO12=1，不重复、不自动停止 | 保留最近成功应用参数 |

Pref标幺基准5000 W：1=5000W，1.1=5500W；数值来自INV_PARAM.test.p_base/p_step。A/B均运行同一CPU ANN预测负载。A全程禁止ANN写H/D；B仅9/24秒尝试一次，其他时间冻结参数。按钮自动设置annJdApplyEnable，操作者不再手工改该开关。INV_CMD.auto_test_enable=1现在是“允许按钮试验”，不是自动开始，默认保持1。

有效性：PLL有效、结果是本段新推理、最近100ms内有推理完成，且AnnAdaptive_Apply原有非有限/正值/Euler条件通过。无效结果跳过本次赋值并置失败标志，后续Pref/GPIO时序不暂停、不重试。失败不等于“已启用自适应”，界面与CSV明确标记。

20秒只取消ANN旧窗口/旧后台结果，不重置运行相角或虚拟功角积分。当前虚拟功角已单独回退原omega_m积分，不含PCC/频率补偿；VSG内部Pe/Qe保持不变。

## 操作

1. 停机确认PWM关闭、断路器断开。CCS使用原已验证的FLASH配置重新构建下载；LaunchPad20MHz可使用artifacts/CPU1_LAUNCHXL_FLASH的新版out。10MHz非LaunchPad导出位于artifacts/PowerCompareFlash，不能混用晶振配置。
2. 打开新版WPF，连接CAN，按原顺序孤岛→预同步→并网。此时Pref=0，ANN关闭，A/B按钮收到新鲜0x203/209和功率数据后可用。
3. 打开“功率实验对比”，填写两组共同HIL条件，点“固定参数启动·A”。无需CCS改Pref/JD。观察进度，40秒后自动保存CSV至上位机exe同目录Experiments。
4. 确认A已保存，使用主界面急停，等待故障/停机反馈，解除急停，再孤岛→预同步→并网。
5. 再次孤岛启动前，DSP在STANDBY恢复上电时的基准H/D（当前H=0.3、D=20），GPIO12恢复0、Pref清零、取消旧ANN结果及试验请求；VSG/虚拟功角沿现有待机路径复位。不会自动重合闸。
6. 点击“自适应参数启动·B”，保持同样HIL初始/切换阻抗、负载、母线。完成后A/B自动叠加，支持手动另存/加载。
7. 记录中不要暂停CPU或修改Pref、GPIO、JD应用命令。需要中止DSP用急停；“仅取消PC记录”以及关闭对比窗口均不是急停，关闭窗口仍会后台采集保存。退出/断连不能替代硬件急停。

改变固定基准参数请修改inverter_settings.c并重新初始化/构建；基准在VsgAutoTest_Init时锁存，两次孤岛启动前均恢复它，避免上一轮自适应值污染固定实验。

## 诊断

VSG_TEST.mode：0等待，1固定A，2自适应B。run_id每次成功启动递增，准备下一轮不归零。start_tick为DSP 100us控制时钟，elapsed_ticks和seconds在40s冻结。
flags：bit0开始时间已锁存；bit1运行；bit2完成；bit3中止；bit4第一次赋值成功；bit5第二次成功；bit6第一次失败；bit7第二次失败。A的成功/失败位保持0。
VSG_TEST.apply_count最多2；J_applied/D_applied为最近一次成功应用快照；实际控制参数看INV_PARAM.vsg.h/dp_pu。预测值看ANN_CTRL.J_apt/D_apt，不能混淆。

完整功率数据、应答协议和存储说明见[POWER_COMPARISON.md](POWER_COMPARISON.md)。仅HIL用途，软件CAN急停不代替硬件保护。
