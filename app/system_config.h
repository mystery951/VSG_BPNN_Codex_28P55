#ifndef SYSTEM_CONFIG_H_
#define SYSTEM_CONFIG_H_
#define SYS_S_BASE_VA 5000.0f
#define SYS_V_BASE_V 155.56f
#define CONTROL_TS 0.0001f
#define TWO_PI 6.28318530718f
#define PWM_PERIOD 7500U
#define MODULATION_LIMIT 0.95f
#define GRID_MIN_PEAK_V 30.0f
#define GRID_PLL_Q_RATIO 0.05f
#define GRID_PLL_MIN_HZ 45.0f
#define GRID_PLL_MAX_HZ 55.0f
#define GRID_PLL_LOCK_SAMPLES 1000U // 100 ms of consecutive stable PLL samples.
#define GRID_PLL_LOCK_DF_HZ 0.02f // Per-sample change of the 10 ms frequency average, not raw jitter.
#define VSG_QV_LIMIT_PU 0.3f
#endif
