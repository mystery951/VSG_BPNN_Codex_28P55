#include "vsg.h"
#include "app/math/pi_controller.h"
#include <math.h>

volatile VSG_DEF VSG_CTRL = {0};
static PI_POS_DEF PI_QV;

static float WrapAngle(float angle)
{
    return atan2f(sinf(angle), cosf(angle));
}

static void ResetAngleBase(volatile VSG_DEF *v)
{
    const VSG_ANGLE_BASE empty = {0};
    v->angle_base = empty;
}

static void ResetVirtualAngle(volatile VSG_DEF *v)
{
    v->virtual_angle_int = v->virtual_angle_rad = v->virtual_angle_deg = 0.0f;
    v->virtual_angle_legacy_int = v->virtual_angle_legacy_deg = 0.0f;
    v->virtual_angle_pcc_int = v->virtual_angle_pcc_deg = 0.0f;
    v->virtual_angle_phase_prev = v->virtual_angle_offset = 0.0f;
    v->virtual_angle_mode = v->virtual_angle_phase_valid = v->virtual_angle_valid = 0U;
    ResetAngleBase(v);
}

void VSG_Reset(volatile VSG_DEF *v)
{
    v->omega_m = v->omega_m_pu = v->omega_sync = 0.0f;
    ResetVirtualAngle(v);
    v->power_loop_cnt = 0U;
    v->q_bias_pu = 0.0f;
    PI_Pos_Reset(&PI_QV);
}

void VSG_Init(volatile VSG_DEF *v)
{
    VSG_Reset(v);
    v->omega0 = v->v_nom = v->v_delta_pu = 0.0f;
    PI_Pos_Init(&PI_QV, 0.0f, 0.0f, VSG_QV_LIMIT_PU, -VSG_QV_LIMIT_PU);
}

void VSG_EnterGrid(volatile VSG_DEF *v, const volatile VSG_PARAM *cfg, float omega0, float omegaSync, float voltage, float qError)
{
    VSG_Reset(v);
    v->omega0 = omega0;
    v->omega_sync = omegaSync;
    v->v_nom = voltage;
    v->v_delta_pu = 0.0f;
    PI_QV.kp = cfg->Kp_QV;
    PI_QV.ki = cfg->Ki_QV;
    v->q_bias_pu = cfg->Ki_QV == 0.0f ? -cfg->Kp_QV * qError : 0.0f;
    PI_QV.ui = cfg->Ki_QV == 0.0f ? 0.0f : -cfg->Kp_QV * qError;
}

float VSG_PowerLoop(volatile VSG_DEF *v, const volatile VSG_PARAM *cfg, float pRef, float qRef, float pe, float qe, float voltageMax, float Ts)
{
    float deviationMax, voltage, requestedVoltage, qError = qRef - qe;
    v->omega_m_pu += (pRef - pe - cfg->dp_pu * v->omega_m_pu) * Ts / (2.0f * cfg->h);
    deviationMax = cfg->omega_limit / v->omega0;
    if(v->omega_m_pu > deviationMax) v->omega_m_pu = deviationMax;
    if(v->omega_m_pu < -deviationMax) v->omega_m_pu = -deviationMax;
    v->omega_m = v->omega_m_pu * v->omega0;
    PI_QV.kp = cfg->Kp_QV;
    PI_QV.ki = cfg->Ki_QV;
    if(cfg->Ki_QV == 0.0f)
    {
        v->v_delta_pu = cfg->Kp_QV * qError + v->q_bias_pu;
        if(v->v_delta_pu > VSG_QV_LIMIT_PU) v->v_delta_pu = VSG_QV_LIMIT_PU;
        if(v->v_delta_pu < -VSG_QV_LIMIT_PU) v->v_delta_pu = -VSG_QV_LIMIT_PU;
    }
    else v->v_delta_pu = PI_Pos_Cal(&PI_QV, qError, Ts);
    voltage = v->v_nom + v->v_delta_pu * SYS_V_BASE_V;
    requestedVoltage = voltage;
    if(voltageMax > cfg->v_max) voltageMax = cfg->v_max;
    if(voltage > voltageMax) voltage = voltageMax;
    if(voltage < cfg->v_min) voltage = cfg->v_min;
    // Track BOTH PI-output and physical voltage saturation; Ki=0 never accumulates.
    if(cfg->Ki_QV != 0.0f)
        PI_QV.ui = (voltage == requestedVoltage ? v->v_delta_pu : (voltage - v->v_nom) / SYS_V_BASE_V) - cfg->Kp_QV * qError;
    return voltage;
}

// Original F28P55 feature: deadband omega_m, integrate radians, wrap only the reported angle.
void VSG_VirtualAngleStep(volatile VSG_DEF *v, float omegaP, float Ts, bool enabled)
{
    if(!enabled)
    {
        ResetVirtualAngle(v);
        return;
    }
    if(omegaP <= 0.015f && omegaP >= -0.015f) omegaP = 0.0f;
    v->virtual_angle_int += omegaP * Ts;
    v->virtual_angle_rad = WrapAngle(v->virtual_angle_int);
    v->virtual_angle_deg = v->virtual_angle_rad * 57.29577951308232f;
    v->virtual_angle_valid = isfinite(v->virtual_angle_int) ? 1U : 0U;
    v->virtual_angle_mode = 0U;
    v->virtual_angle_legacy_int = v->virtual_angle_int;
    v->virtual_angle_legacy_deg = v->virtual_angle_deg;
}

// Compatibility entry points only: PCC increments/baseline compensation have been removed.
void VSG_VirtualAngleUpdate(volatile VSG_DEF *v, float thetaInv, float thetaPcc, float Ts, bool enabled, bool pccValid, uint16_t mode)
{
    (void)thetaInv; (void)thetaPcc; (void)pccValid; (void)mode;
    VSG_VirtualAngleStep(v, v->omega_m, Ts, enabled);
}

bool VSG_VirtualAngleEstablish(volatile VSG_DEF *v, const volatile VIRTUAL_ANGLE_PARAM *cfg, float Ts, bool pllLocked, float frequency, float p, float q, float pRef, float qRef)
{
    (void)v; (void)cfg; (void)Ts; (void)pllLocked; (void)frequency;
    (void)p; (void)q; (void)pRef; (void)qRef;
    return false; // No automatic baseline latch or ANN-window cancellation.
}
