#include "Control/Controller.h"
#include "Utils/Math.hpp"
#include <algorithm>

namespace Espfc::Control {

Controller::Controller(Model& model): _model(model), _rates{}, _altHoldLatched(false), _altHoldTarget(0.0f), _hoverThrottle(0.0f), _altHoldActivePrev(false),
  _brakeVelX(0.0f), _brakeVelY(0.0f), _brakeCorrPitch(0.0f), _brakeCorrRoll(0.0f), _brakeDt(0.0f), _brakeWasStickCentered(false) {}

int Controller::begin()
{
  _rates.begin(_model.config.input);
  _speedFilter.begin(FilterConfig(FILTER_BIQUAD, 10), _model.state.loopTimer.rate);

  beginInnerLoop(AXIS_ROLL);
  beginInnerLoop(AXIS_PITCH);
  beginInnerLoop(AXIS_YAW);
  beginOuterLoop(AXIS_ROLL);
  beginOuterLoop(AXIS_PITCH);
  beginAltHold();
  _model.state.innerPid[AXIS_THRUST].iTerm = _model.state.innerPid[AXIS_THRUST].iReset; // boot-time hover seed
  beginInertialBrake();

  return 1;
}

int FAST_CODE_ATTR Controller::update()
{
  if (_model.state.reloadPidPending)
  {
    _model.state.reloadPidPending = false;
    reloadPidGains();
  }

  uint32_t startTime = 0;
  if (_model.config.debug.mode == DEBUG_PIDLOOP)
  {
    startTime = micros();
    _model.state.debug[0] = startTime - _model.state.loopTimer.last;
  }

  {
    Utils::Stats::Measure(_model.state.stats, COUNTER_OUTER_PID);
    resetIterm();
    switch (_model.config.mixer.type)
    {
      case FC_MIXER_GIMBAL: outerLoopRobot(); break;

      default: outerLoop(); break;
    }
  }

  {
    Utils::Stats::Measure(_model.state.stats, COUNTER_INNER_PID);
    switch (_model.config.mixer.type)
    {
      case FC_MIXER_GIMBAL: innerLoopRobot(); break;

      default: innerLoop(); break;
    }
  }

  if (_model.config.debug.mode == DEBUG_PIDLOOP)
  {
    _model.state.debug[2] = micros() - startTime;
  }

  return 1;
}

void Controller::outerLoopRobot()
{
  const float speedScale = 2.f;
  const float gyroScale = 0.1f;
  const float speed = _speedFilter.update(_model.state.output.ch[AXIS_PITCH] * speedScale +
                                          _model.state.gyro.adc[AXIS_PITCH] * gyroScale);
  float angle = 0;
  const auto& input = _model.state.input;
  const auto& levelConf = _model.config.level;

  if (true || _model.isModeActive(MODE_ANGLE))
  {
    angle = input.ch[AXIS_PITCH] * Utils::toRad(levelConf.angleLimit);
  }
  else
  {
    angle = _model.state.outerPid[AXIS_PITCH].update(input.ch[AXIS_PITCH], speed) * Utils::toRad(levelConf.rateLimit);
  }
  _model.state.setpoint.angle.set(AXIS_PITCH, angle);
  _model.state.setpoint.rate[AXIS_YAW] = input.ch[AXIS_YAW] * Utils::toRad(levelConf.rateLimit);

  if (_model.config.debug.mode == DEBUG_ANGLERATE)
  {
    _model.state.debug[0] = speed * 1000;
    _model.state.debug[1] = lrintf(Utils::toDeg(angle) * 10);
  }
}

void Controller::innerLoopRobot()
{
  // VectorFloat v(0.f, 0.f, 1.f);
  // v.rotate(_model.state.attitude.quaternion);
  // const float angle = acos(v.z);

  const auto& attitude = _model.state.attitude;
  const auto& setpoint = _model.state.setpoint;

  auto& output = _model.state.output;
  auto& innerPid = _model.state.innerPid;

  const float angle = std::max(abs(attitude.euler[AXIS_PITCH]), abs(attitude.euler[AXIS_ROLL]));
  const bool stabilize = angle < Utils::toRad(_model.config.level.angleLimit);
  if (stabilize)
  {
    output.ch[AXIS_PITCH] = innerPid[AXIS_PITCH].update(setpoint.angle[AXIS_PITCH], attitude.euler[AXIS_PITCH]);
    output.ch[AXIS_YAW] = innerPid[AXIS_YAW].update(setpoint.rate[AXIS_YAW], _model.state.gyro.adc[AXIS_YAW]);
  }
  else
  {
    resetIterm();
    output.ch[AXIS_PITCH] = 0.f;
    output.ch[AXIS_YAW] = 0.f;
  }

  if (_model.config.debug.mode == DEBUG_ANGLERATE)
  {
    _model.state.debug[2] = lrintf(Utils::toDeg(attitude.euler[AXIS_PITCH]) * 10);
    _model.state.debug[3] = lrintf(output.ch[AXIS_PITCH] * 1000);
  }
}

void FAST_CODE_ATTR Controller::outerLoop()
{
  // Roll/Pitch rates control
  if (_model.isModeActive(MODE_ANGLE))
  {
    // Run Active Inertial Velocity Braking observer every frame
    updateInertialBrake();

    for (size_t i = 0; i < AXIS_COUNT_RP; i++)
    {
      float angleSetpoint = Utils::toRad(_model.config.level.angleLimit) * _model.state.input.ch[i];

      // Inject inertial braking correction when stick is centered
      // Pitch axis (i==1) uses _brakeCorrPitch, Roll axis (i==0) uses _brakeCorrRoll
      if (i == AXIS_PITCH)
        angleSetpoint += _brakeCorrPitch;
      else
        angleSetpoint += _brakeCorrRoll;

      _model.state.setpoint.rate[i] = _model.state.outerPid[i].update(angleSetpoint, _model.state.attitude.euler[i]);
      // disable fterm in angle mode
      _model.state.innerPid[i].fScale = 0.f;
    }
  }
  else
  {
    for (size_t i = 0; i < AXIS_COUNT_RP; i++)
    {
      _model.state.setpoint.rate[i] = calculateSetpointRate(i, _model.state.input.ch[i]);
    }
  }

  // Yaw rates control
  _model.state.setpoint.rate[AXIS_YAW] = calculateSetpointRate(AXIS_YAW, _model.state.input.ch[AXIS_YAW]);

  // thrust control
  bool altHoldActive = _model.isModeActive(MODE_ALTHOLD) && _model.isModeActive(MODE_ARMED);
  if (altHoldActive)
  {
    _model.state.setpoint.rate[AXIS_THRUST] = calcualteAltHoldSetpoint();
  }
  else
  {
    _model.state.setpoint.rate[AXIS_THRUST] = _model.state.input.ch[AXIS_THRUST];
    _altHoldActivePrev = false;
  }

  // debug
  if (_model.config.debug.mode == DEBUG_ANGLERATE)
  {
    for (size_t i = 0; i < AXIS_COUNT_RPY; ++i)
    {
      _model.state.debug[i] = lrintf(Utils::toDeg(_model.state.setpoint.rate[i]));
    }
  }
}

void FAST_CODE_ATTR Controller::innerLoop()
{
  // Roll/Pitch/Yaw rates control
  const float tpaFactor = getTpaFactor();
  const auto& setpoint = _model.state.setpoint;
  const auto& altitude = _model.state.altitude;

  auto& innerPid = _model.state.innerPid;
  auto& output = _model.state.output;

  for (size_t i = 0; i < AXIS_COUNT_RPY; ++i)
  {
    output.ch[i] = innerPid[i].update(setpoint.rate[i], _model.state.gyro.adc[i]) * tpaFactor;
  }

  // thrust control
  bool altHoldActive = _model.isModeActive(MODE_ALTHOLD) && _model.isModeActive(MODE_ARMED);

  if (altHoldActive)
  {
    // If throttle stick is at minimum and drone is resting on ground, cut motors for safety
    if (_model.state.input.ch[AXIS_THRUST] < -0.92f && altitude.height < 0.15f)
    {
      output.ch[AXIS_THRUST] = -1.0f;
      innerPid[AXIS_THRUST].iTerm = 0.10f; // Seed hover baseline for next liftoff
    }
    else
    {
      float thrust = innerPid[AXIS_THRUST].update(setpoint.rate[AXIS_THRUST], altitude.vario);
      float cosTheta = std::clamp(_model.state.attitude.cosTheta, 0.80f, 1.0f); // Tilt boost

      // Battery voltage sag compensation: maintains consistent hover lift as LiPo discharges
      float vbatComp = 1.0f;
      if (_model.state.battery.cells > 0 && _model.state.battery.samples == 0)
      {
        const float cellV = _model.state.battery.cellVoltage;
        if (cellV >= 2.80f && cellV <= 4.40f)
        {
          vbatComp = 4.00f / cellV;
          vbatComp = std::clamp(vbatComp, 0.90f, 1.25f);
        }
      }

      thrust = (thrust + 1.0f) * vbatComp / cosTheta - 1.0f;
      output.ch[AXIS_THRUST] = std::clamp(thrust, -1.0f, 1.0f);
    }
  }
  else
  {
    // Freeze the thrust PID while althold is off: integrating (0, vario) here would wind
    // I-term against manual climbs/descents and poison the hover seed on activation.
    innerPid[AXIS_THRUST].error = 0.f;
    output.ch[AXIS_THRUST] = _model.state.input.ch[AXIS_THRUST];
  }

  // Store altitude PID debug signals when in altitude/pidloop debug modes
  if (_model.config.debug.mode == DEBUG_ALTITUDE || _model.config.debug.mode == DEBUG_NONE)
  {
    _model.state.debug[0] = std::clamp(lrintf(setpoint.rate[AXIS_THRUST] * 1000.0f), -3000l, 3000l);
    _model.state.debug[1] = std::clamp(lrintf(altitude.vario * 1000.0f), -30000l, 30000l);
    _model.state.debug[2] = std::clamp(lrintf(altitude.height * 100.0f), -30000l, 30000l);
    _model.state.debug[3] = std::clamp(lrintf(innerPid[AXIS_THRUST].error * 1000.0f), -30000l, 30000l);
    _model.state.debug[4] = std::clamp(lrintf(innerPid[AXIS_THRUST].pTerm * 1000.0f), -3000l, 3000l);
    _model.state.debug[5] = std::clamp(lrintf(innerPid[AXIS_THRUST].iTerm * 1000.0f), -3000l, 3000l);
    _model.state.debug[6] = std::clamp(lrintf(innerPid[AXIS_THRUST].dTerm * 1000.0f), -3000l, 3000l);
    _model.state.debug[7] = std::clamp(lrintf(innerPid[AXIS_THRUST].fTerm * 1000.0f), -3000l, 3000l);
  }
}

float Controller::calcualteAltHoldSetpoint()
{
  bool altHoldActive = _model.isModeActive(MODE_ALTHOLD);

  float cosTilt = std::max(_model.state.attitude.cosTheta, 0.6f);
  float currentTrueAlt = (_model.state.rangefinder.valid && _model.state.rangefinder.distance >= 0.03f) ?
                         (_model.state.rangefinder.distance * cosTilt) :
                         _model.state.altitude.height;

  // 1. Initial activation: lock current altitude as starting target
  if (altHoldActive && !_altHoldActivePrev)
  {
    if (currentTrueAlt <= 0.15f)
    {
      _altHoldTarget = 0.30f; // Initial hover target = 30 cm (safely above ground effect)
      _hoverThrottle = 0.10f; // ~55% hover baseline for brushed micro-drones (guarantees takeoff lift)
      _model.state.innerPid[AXIS_THRUST].iTerm = _hoverThrottle;
    }
    else
    {
      _altHoldTarget = std::clamp(currentTrueAlt, 0.15f, 1.20f);
      _hoverThrottle = std::clamp(_model.state.innerPid[AXIS_THRUST].iTerm, -0.05f, 0.30f);
      _model.state.innerPid[AXIS_THRUST].iTerm = _hoverThrottle;
    }
    _altHoldLatched = true;
  }
  _altHoldActivePrev = altHoldActive;

  const float ch = _model.state.input.ch[AXIS_THRUST];
  static uint32_t lastAltHoldTime = 0;
  uint32_t now = micros();
  float dt = (lastAltHoldTime > 0) ? ((now - lastAltHoldTime) * 1e-6f) : 0.002f;
  dt = std::clamp(dt, 0.0005f, 0.050f);
  lastAltHoldTime = now;

  // 2. Continuous Target Integrator with Rate Feedforward:
  // Stick deflection shifts the TARGET itself; releasing the stick freezes it at the new height.
  float shiftRate = 0.0f;
  if (ch > 0.15f)
  {
    shiftRate = ((ch - 0.15f) / 0.85f) * 0.50f;   // up to +0.50 m/s
  }
  else if (ch < -0.15f)
  {
    shiftRate = ((ch + 0.15f) / 0.85f) * 0.40f;   // negative -> down to -0.40 m/s
  }
  _altHoldTarget += shiftRate * dt;

  // Constrain altitude target safely within sensor operational range [15cm, 1.20m]
  _altHoldTarget = std::clamp(_altHoldTarget, 0.15f, 1.20f);

  // 3. Position P + Rate Feedforward:
  // FF carries the steady-state demand so height tracks the shifting target with no lag,
  // and the drone stops climbing the moment the stick is released.
  const float error = _altHoldTarget - _model.state.altitude.height;
  const float velSetpoint = error * 0.70f + shiftRate;
  return std::clamp(velSetpoint, -0.45f, 0.65f);
}


float Controller::getTpaFactor() const
{
  if (_model.config.controller.tpaScale == 0) return 1.f;
  float t = Utils::clamp(_model.state.input.us[AXIS_THRUST], (float)_model.config.controller.tpaBreakpoint, 2000.f);
  return Utils::map(t, (float)_model.config.controller.tpaBreakpoint, 2000.f, 1.f,
                    1.f - ((float)_model.config.controller.tpaScale * 0.01f));
}

void Controller::resetIterm()
{
  if (!_model.isModeActive(MODE_ARMED) // when not armed
      || (!_model.isAirModeActive() && _model.config.iterm.lowThrottleZeroIterm &&
          _model.isThrottleLow()) // on low throttle (not in air mode)
  )
  {
    for (size_t i = 0; i < AXIS_COUNT_RPY; i++)
    {
      _model.state.innerPid[i].resetIterm();
      _model.state.outerPid[i].resetIterm();
    }
  }
  if (!_model.isModeActive(MODE_ARMED))
  {
    //_model.state.innerPid[AXIS_THRUST].resetIterm();
  }
}

float Controller::calculateSetpointRate(int axis, float input) const
{
  if (axis == AXIS_YAW) input *= -1.f;
  return _rates.getSetpoint(axis, input);
}

void Controller::beginInnerLoop(size_t axis)
{
  const int pidFilterRate = _model.state.loopTimer.rate;
  float pidScale[] = {1.f, 1.f, 1.f};
  if (_model.config.mixer.type == FC_MIXER_GIMBAL)
  {
    pidScale[AXIS_YAW] = 0.2f;   // ROBOT
    pidScale[AXIS_PITCH] = 20.f; // ROBOT
  }

  const auto& pc = _model.config.pid[axis];
  const auto& dtermConf = _model.config.dterm;

  auto& pid = _model.state.innerPid[axis];
  pid.Kp = (float)pc.P * PTERM_SCALE * pidScale[axis];
  pid.Ki = (float)pc.I * ITERM_SCALE * pidScale[axis];
  pid.Kd = (float)pc.D * DTERM_SCALE * pidScale[axis];
  pid.Kf = (float)pc.F * FTERM_SCALE * pidScale[axis];
  pid.iLimitLow = -_model.config.iterm.limit * 0.01f;
  pid.iLimitHigh = _model.config.iterm.limit * 0.01f;
  pid.oLimitLow = -0.66f;
  pid.oLimitHigh = 0.66f;
  pid.rate = pidFilterRate;
  pid.dtermNotchFilter.begin(dtermConf.notchFilter, pidFilterRate);
  if (dtermConf.dynLpfFilter.cutoff > 0)
  {
    pid.dtermFilter.begin(FilterConfig((FilterType)dtermConf.filter.type, dtermConf.dynLpfFilter.cutoff),
                          pidFilterRate);
  }
  else
  {
    pid.dtermFilter.begin(dtermConf.filter, pidFilterRate);
  }
  pid.dtermFilter2.begin(dtermConf.filter2, pidFilterRate);
  pid.ftermFilter.begin(_model.config.input.filterDerivative, pidFilterRate);
  pid.itermRelaxFilter.begin(FilterConfig(FILTER_PT1, _model.config.iterm.relaxCutoff), pidFilterRate);
  if (axis == AXIS_YAW)
  {
    pid.itermRelax = (_model.config.iterm.relax == ITERM_RELAX_RPY || _model.config.iterm.relax == ITERM_RELAX_RPY_INC)
                         ? _model.config.iterm.relax
                         : ITERM_RELAX_OFF;
    pid.ptermFilter.begin(_model.config.yaw.filter, pidFilterRate);
  }
  else
  {
    pid.itermRelax = _model.config.iterm.relax;
  }
  pid.begin();
}

void Controller::beginOuterLoop(size_t axis)
{
  const int pidFilterRate = _model.state.loopTimer.rate;
  const auto& pc = _model.config.pid[FC_PID_LEVEL];

  auto& pid = _model.state.outerPid[axis];
  float pVal = (pc.P > 0) ? (float)pc.P : 60.0f; // Guaranteed Kp = 60.0 Level gain
  pid.Kp = pVal * LEVEL_PTERM_SCALE;
  pid.Ki = (float)pc.I * LEVEL_ITERM_SCALE;
  pid.Kd = (float)pc.D * LEVEL_DTERM_SCALE;
  pid.Kf = (float)pc.F * LEVEL_FTERM_SCALE;
  pid.iLimitHigh = Utils::toRad(_model.config.level.rateLimit * 0.1f);
  pid.iLimitLow = -pid.iLimitHigh;
  pid.oLimitHigh = Utils::toRad(_model.config.level.rateLimit);
  pid.oLimitLow = -pid.oLimitHigh;
  pid.rate = pidFilterRate;
  pid.ptermFilter.begin(_model.config.level.ptermFilter, pidFilterRate);
  pid.begin();
}

void Controller::beginAltHold()
{
  const auto& pc = _model.config.pid[FC_PID_VEL];
  auto& pid = _model.state.innerPid[AXIS_THRUST];
  float pVal = (pc.P > 0) ? (float)pc.P : 28.0f;
  float iVal = (pc.I > 0) ? (float)pc.I : 22.0f;
  float dVal = (pc.D > 0) ? (float)pc.D : 8.0f;
  float fVal = (pc.F > 0) ? (float)pc.F : 0.0f;

  pid.Kp = pVal * 0.0100f; // Responsive velocity damping (0.28)
  pid.Ki = iVal * 0.0100f; // Stable integral hover convergence (0.22)
  pid.Kd = dVal * 0.0010f; // Velocity D-term damping (0.008) - arrests vertical momentum!
  pid.Kf = fVal * 0.0100f; // Zero feedforward kick
  pid.iLimitLow = -0.40f;  // ~30% minimum thrust
  pid.iLimitHigh = 0.65f;  // ~82.5% maximum thrust
  pid.iReset = 0.10f;      // ~55% hover throttle baseline
  pid.rate = _model.state.loopTimer.rate;
  pid.ptermFilter.begin(FilterConfig(FILTER_PT1, 40), _model.state.loopTimer.rate); // 40Hz low phase-lag filter
  pid.dtermFilter.begin(FilterConfig(FILTER_PT1, 30), _model.state.loopTimer.rate); // 30Hz D-term filter
  pid.ftermDerivative = false;
  pid.begin();
}

void Controller::reloadPidGains()
{
  beginInnerLoop(AXIS_ROLL);
  beginInnerLoop(AXIS_PITCH);
  beginInnerLoop(AXIS_YAW);
  beginOuterLoop(AXIS_ROLL);
  beginOuterLoop(AXIS_PITCH);
  beginAltHold();
}

void Controller::beginInertialBrake()
{
  _brakeDt = 1.0f / (float)_model.state.loopTimer.rate;
  _brakeVelX = 0.0f;
  _brakeVelY = 0.0f;
  _brakeCorrPitch = 0.0f;
  _brakeCorrRoll = 0.0f;
  _brakeWasStickCentered = false;

  // High-pass filter at 0.5 Hz: passes momentum transients, rejects DC accel bias drift
  // This is the key trick — by high-pass filtering the integrated velocity, we get
  // ~2 seconds of usable velocity data before bias accumulates, which is plenty for braking
  _brakeHpfX.begin(FilterConfig(FILTER_PT1, 80), _model.state.loopTimer.rate);
  _brakeHpfY.begin(FilterConfig(FILTER_PT1, 80), _model.state.loopTimer.rate);

  // Low-pass output filter at 5 Hz: smooth braking corrections to prevent jitter
  _brakeLpfX.begin(FilterConfig(FILTER_PT1, 5), _model.state.loopTimer.rate);
  _brakeLpfY.begin(FilterConfig(FILTER_PT1, 5), _model.state.loopTimer.rate);
}

void FAST_CODE_ATTR Controller::updateInertialBrake()
{
  // TEMP: disabled while validating laser AltHold.
  // The observer below zeroes _brakeVel on every stick-deflected frame, so at stick
  // release it starts from 0 and integrates only the deceleration — the resulting
  // correction has the wrong sign and tilts into the direction of travel. Leaving the
  // body intact for reference; re-enable only after the sign and the HPF are fixed.
  _brakeCorrPitch = 0.0f;
  _brakeCorrRoll  = 0.0f;
  return;

  // Only active when armed and in angle mode
  if (!_model.isModeActive(MODE_ARMED) || !_model.isModeActive(MODE_ANGLE))
  {
    _brakeVelX = 0.0f;
    _brakeVelY = 0.0f;
    _brakeCorrPitch = 0.0f;
    _brakeCorrRoll = 0.0f;
    _brakeWasStickCentered = false;
    return;
  }

  // --- 1. Short-Term Velocity Observer ---
  // Integrate world-frame acceleration to get raw velocity (m/s)
  // accel.world is in m/s² with gravity already removed (see Fusion.cpp line 73)
  const float accX = _model.state.accel.world.x;  // Forward/backward (maps to pitch)
  const float accY = _model.state.accel.world.y;  // Left/right (maps to roll)

  _brakeVelX += accX * _brakeDt;
  _brakeVelY += accY * _brakeDt;

  // High-pass filter the velocity to reject DC bias drift
  // Without this, accel bias would cause velocity to ramp linearly forever.
  // With a ~0.5Hz HPF, we get clean velocity for ~2 seconds — more than enough for braking.
  float velXhp = _brakeHpfX.update(_brakeVelX);
  float velYhp = _brakeHpfY.update(_brakeVelY);

  // --- 2. Stick-Centered Detection ---
  // Deadband: consider sticks "centered" when both roll and pitch are within ±5% of center
  const float rollInput  = _model.state.input.ch[AXIS_ROLL];
  const float pitchInput = _model.state.input.ch[AXIS_PITCH];
  const float stickDeadband = 0.05f;
  bool stickCentered = (std::abs(rollInput) < stickDeadband) && (std::abs(pitchInput) < stickDeadband);

  // --- 3. Braking Logic ---
  if (stickCentered)
  {
    // Velocity threshold: only brake if we detect meaningful momentum (>0.08 m/s)
    // Below this, vibration noise dominates and we'd just add jitter
    constexpr float velThreshold = 0.08f;  // m/s minimum to trigger braking
    constexpr float brakeGain = 0.035f;    // radians per (m/s) — how aggressively to counter-tilt
    constexpr float maxBrakeAngle = Utils::toRad(2.5f); // ±2.5° maximum braking tilt
    constexpr float decayRate = 0.92f;     // Exponential decay per frame (~250ms at 500Hz)

    // Generate braking correction: tilt OPPOSITE to velocity direction
    float rawBrakePitch = 0.0f;
    float rawBrakeRoll  = 0.0f;

    if (std::abs(velXhp) > velThreshold)
    {
      rawBrakePitch = -velXhp * brakeGain;  // Negative: tilt opposite to velocity
    }
    if (std::abs(velYhp) > velThreshold)
    {
      rawBrakeRoll = -velYhp * brakeGain;
    }

    // Clamp to safe maximum braking angle
    rawBrakePitch = std::clamp(rawBrakePitch, -maxBrakeAngle, maxBrakeAngle);
    rawBrakeRoll  = std::clamp(rawBrakeRoll,  -maxBrakeAngle, maxBrakeAngle);

    // Smooth with low-pass filter to prevent sharp jerks
    _brakeCorrPitch = _brakeLpfX.update(rawBrakePitch);
    _brakeCorrRoll  = _brakeLpfY.update(rawBrakeRoll);

    // Exponential decay: braking fades out naturally as the drone decelerates
    _brakeCorrPitch *= decayRate;
    _brakeCorrRoll  *= decayRate;
  }
  else
  {
    // Stick is active — pilot is flying manually, zero all braking immediately
    _brakeCorrPitch = 0.0f;
    _brakeCorrRoll  = 0.0f;
    _brakeLpfX.update(0.0f); // Flush filter state
    _brakeLpfY.update(0.0f);

    // Reset velocity integrators when stick re-centers to start with a clean slate
    if (!_brakeWasStickCentered)
    {
      _brakeVelX = 0.0f;
      _brakeVelY = 0.0f;
    }
  }

  _brakeWasStickCentered = stickCentered;
}

} // namespace Espfc::Control
