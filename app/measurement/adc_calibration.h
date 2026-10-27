#ifndef ADC_CALIBRATION_H_
#define ADC_CALIBRATION_H_

// Fixed offsets measured with the platform at zero physical input.
// Valid for the internal 3.3 V ADC range and the current platform gains.
// Indices follow physical signals, not pin order. Recheck offsets after rewiring.
#define ADC_ZERO_CHANNELS  13U

static const float adcZeroOffset[ADC_ZERO_CHANNELS] =
{
    1855.0f, // [0]  ua_cap    A0
    1855.0f, // [1]  PCC_ua    E15
    1855.0f, // [2]  ia_inv    A5
    1858.0f, // [3]  ub_cap    E12
    1855.0f, // [4]  PCC_ub    B1
    1853.0f, // [5]  ib_inv    B8
    1858.0f, // [6]  uc_cap    C4
    1855.0f, // [7]  PCC_uc    C8
    1859.0f, // [8]  ic_inv    C14
    1855.0f, // [9]  ia_filter A12
    1859.0f, // [10] ib_filter B10
    1857.0f, // [11] ic_filter D11
    1858.0f  // [12] P_filter  D12
};

#define VOL_SCALE \
    (3.3f / 4096.0f * (20.0f / 3.0f) \
     / 50.0f * 1000.0f * 1.0074f)
//1.003
#define CUR_SCALE \
    (3.3f / 4096.0f * (20.0f / 3.0f) \
     / 200.0f * 1000.0f * 1.002f)

// Power in W per ADC count; single-precision and no integer division.
#define POWER_SCALE (3.3f / 4096.0f * (20.0f / 3.0f) * (1.0f / 1000.0f) * 1000000.0f)

#endif
