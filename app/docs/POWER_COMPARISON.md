# P_filter 1ms遥测与按钮40秒A/B记录

2026-10-23：当前版替代首次6秒/手动Pref触发版。原监控、PLL、VSG、ADC转换、ANN模型与五种CAN控制命令保留；新增05/06启动A/B及209实验应答。旧WinForms仍能执行原命令，但启动试验应使用新版WPF。

## 协议（MCANB GPIO58 TX/GPIO33 RX，经典标准帧1Mbps，小端）

| ID | DLC/周期 | 字节0～3 | 字节4～7 |
| --- | --- | --- | --- |
| 0x206 | 8 / 1ms采样 | uint32控制tick（100us单位） | float32 Pref_W |
| 0x207 | 8 / 1ms采样 | 与206匹配的tick | float32 P_filter_W（HIL D12） |
| 0x208 | 8 / 100ms | float32实际采用J=2H | float32实际采用D |
| 0x209 | 8 / 100ms | uint32实验开始tick | run_id低/高字节、mode、flags |

0x200命令仍DLC4、命令 A5 5A 00；05固定A，06自适应B。其他01/02/03/04、独立0x100 FF急停格式不变。新命令由DSP检查实际已并网、按钮实验使能、未启动过本轮；运行/完成/急停中拒绝重复开始。并网03在任何auto_test_enable设置下都不自动给Pref=1；重复03不改变已运行的Pref/时序。

P_filter来自ADC D12换算W，不是INV_FEEDBACK.Pe；VSG内部仍使用原Pe/Qe，未更换控制反馈。P_filter是否经过HIL滤波、输出周期及测量位置由平台模型定义，1ms采样不能恢复被滤掉的动态。0x201～205保持原100ms布局和功能。

209 mode=0等待、1固定A、2自适应B；flags bit0已锁存起点、bit1运行、bit2完成、bit3中止、bit4/5第一次/第二次赋值成功、bit6/7第一次/第二次赋值失败。等待时mode/flags=0，run_id保留上一轮编号。重启固件run_id从0开始。

## 时基与队列

10kHz控制ISR末尾捕获功率，每10拍一次。启动当拍强制额外捕获t=0样本并重新对齐1ms分频；同时锁存209开始tick，40秒理想记录40001点。中断只入队，不等待MCAN。206/207按相同tick配对，不用PC到达时间或鼠标时间对齐。
63个可用环形队列槽，约63ms积压；主循环每次最多尝试4对、非阻塞，半对失败保留同一样本重试。队列满丢新样本并计数。TX槽当前11个（含新增无功模式/Q反馈），无额外引脚/SysConfig修改。
两帧/ms约2000帧/s，加低速九帧90帧/s，总约2090帧/s。按每帧160bit保守预算约34%总线占用，不含其他节点和错误重传。这不是现场带宽测量。
CCS监视cana_power_sample_count（每秒约1000）、cana_power_tx_count（提交完整对数，非ACK数）、cana_power_drop_count（应不增加）、cana_power_queue_peak（最大63）、cana_tx_count（硬件ACK累计）。busy_count含非阻塞重试，不等于丢样。

1ms是DSP采样周期，不承诺USB/Windows逐毫秒交付或界面逐毫秒刷新。UI约100ms更新状态、约500ms更新进行中曲线；保存完整原始点，不平滑、不补点，缺口不跨线。HIL实测要确认丢样计数不增、时间戳连续。

## 40秒流程与保存

详见[VSG_AUTO_TEST.md](VSG_AUTO_TEST.md)。两个按钮会实际发送CAN、启动Pref/GPIO变化，不再只是A/B标签。
0s Pref=1；9s B第一次整定；10s Pref=1.1；15s Pref=1；20s GPIO12=1；24s B第二次整定；25s Pref=1.1；30s Pref=1；40s结束记录，保持Pref=1和GPIO12=1。失败整定不拖延时序。
A完成后急停→解除→重新孤岛→预同步→并网→B。下一次孤岛前恢复相同基准H/D与GPIO12=0。必须保持同样HIL初始/切换阻抗、负载及母线。

PC先准备采集再发05/06，等209中新run_id/对应mode和起点才建立本轮记录。约2.5秒近期缓存可回收先到功率、后到应答的起点数据。重复点击禁用；未收到应答约2秒、功率中断500ms或实验状态中断1000ms取消PC记录并提示DSP可能仍在运行，不自动重发。
40秒数据收齐且DSP报告完成才作为完整记录替换A/B。早停/掉线/重启不覆盖已有完整记录。完成队列由主窗口后台保存至exe目录Experiments，文件名含A/B、日期、run_id、唯一后缀。失败保留内存记录并提示手动另存。关闭对比窗口仍记录，关闭整个程序会断开通信，不能代替停止DSP；完整文件保存任务在退出前等待完成。

CSV V2保存来源P_filter、实验编号/flags、备注UTC、开始tick、逐点Pref/P、100ms预测R/L/J/D和实际J/D最近接收值及各自age_ms。慢值保持不代表1ms新测量，年龄是PC接收年龄。未收到为NaN，原始非有限值保留。支持读取旧V1 Pe短记录，界面标明来源，不能当作P_filter同源对比。
两组叠加共用0～40s横轴和kW纵轴，保留各自Pref曲线。应用成功/失败以DSP flags显示，不以预测值变化推断。CSV加载上限50000点/25MB。

## 构建与验证

新版20MHz LaunchPad：tests/run_target_build.ps1 -LaunchPad -Flash -ExportDirectory artifacts/CPU1_LAUNCHXL_FLASH。10MHz通用配置可不加LaunchPad导出到artifacts/PowerCompareFlash。晶振必须与板卡一致。
已验证：TI FLASH编译链接，CAN命令/1ms队列/强制零点/100ms兼容，A/B相同时序、9/24秒应用与失败、不自动开始/重复开始/急停及基准恢复；ANN/PLL/虚拟功角专项；WPF启动、命令、40秒40001点、延迟应答、缺包/NaN、CSV回读与窗口布局。
未连接或烧写硬件、未向实物发送CAN；总回归旧TestVsgPowerUnits仍有2H写死为5而当前H=0.3的历史夹具差异，本次未改控制参数来迁就该测试。不能将专项通过描述为旧总回归全部通过。预览图片为标明的合成测试，非实验性能。
