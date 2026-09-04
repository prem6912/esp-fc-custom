#pragma once

#include "Control/Altitude.hpp"
#include "Control/Rates.h"
#include "Model.h"
#include "Utils/Filter.h"

namespace Espfc::Control {

class Controller
{
public:
  Controller(Model& model);
  int begin();
  int update();

  void outerLoopRobot();
  void innerLoopRobot();
  void outerLoop();
  void innerLoop();

  inline float getTpaFactor() const;
  inline void resetIterm();
  void reloadPidGains();
  float calculateSetpointRate(int axis, float input) const;
  float calcualteAltHoldSetpoint();

private:
  void beginAltHold();
  void beginInnerLoop(size_t axis);
  void beginOuterLoop(size_t axis);
  void beginInertialBrake();
  void updateInertialBrake();

  Model& _model;
  Rates _rates;
  Utils::Filter _speedFilter;
  bool _altHoldLatched;
  float _altHoldTarget;
  float _hoverThrottle;
  bool _altHoldActivePrev;

  // Active Inertial Velocity Braking (Virtual Air Brake)
  float _brakeVelX;          // High-pass filtered velocity estimate (world X → pitch axis)
  float _brakeVelY;          // High-pass filtered velocity estimate (world Y → roll axis)
  float _brakeCorrPitch;     // Current braking angle correction output for pitch (radians)
  float _brakeCorrRoll;      // Current braking angle correction output for roll (radians)
  float _brakeDt;            // Loop period (seconds)
  bool  _brakeWasStickCentered; // Previous frame stick-centered state (for edge detection)
  Utils::Filter _brakeHpfX;  // High-pass filter for X velocity (rejects DC drift)
  Utils::Filter _brakeHpfY;  // High-pass filter for Y velocity (rejects DC drift)
  Utils::Filter _brakeLpfX;  // Low-pass filter to smooth braking output
  Utils::Filter _brakeLpfY;  // Low-pass filter to smooth braking output
};

} // namespace Espfc::Control
