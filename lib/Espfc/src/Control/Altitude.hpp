#pragma once

#include "Model.h"
#include "Utils/Filter.h"

namespace Espfc::Control {

class Altitude
{
public:
  Altitude(Model& model): _model(model) {}

  int begin()
  {
    _model.state.altitude.height = 0.0f;
    _model.state.altitude.vario = 0.0f;
    _estHeight = 0.0f;
    _estVario = 0.0f;
    _lastBaroAlt = 0.0f;
    _initialized = false;
    return 1;
  }

  int update()
  {
    float dt = 1.0f / _model.state.accel.timer.rate;
    float baroAlt = _model.state.baro.altitudeGround;

    if (!_initialized)
    {
      _estHeight = baroAlt;
      _estVario = _model.state.baro.vario;
      _lastBaroAlt = baroAlt;
      _initialized = true;
    }

    // Get linear vertical acceleration in world frame
    const Quaternion& q = _model.state.attitude.quaternion;
    VectorFloat accelBody = _model.state.accel.adc;
    VectorFloat accelWorld = accelBody.getRotated(q.getConjugate());
    
    // Z acceleration minus gravity (1.0G) in earth frame
    float accelZ = (accelWorld.z - 1.0f) * ACCEL_G;

    // Apply deadband to accelZ to prevent drift on table
    if (std::abs(accelZ) < 0.15f)
    {
      accelZ = 0.0f;
    }

    // Predict step
    _estHeight += _estVario * dt + 0.5f * accelZ * dt * dt;
    _estVario += accelZ * dt;

    // If new baro reading, apply correction
    if (baroAlt != _lastBaroAlt)
    {
      float errorHeight = baroAlt - _estHeight;
      _lastBaroAlt = baroAlt;

      // Time constant (tau = 1.0 seconds) for complementary filter
      float tau = 1.0f; 
      float k1 = dt / tau;
      float k2 = dt / (tau * tau);

      _estHeight += k1 * errorHeight;
      _estVario += k2 * errorHeight;
    }

    _model.state.altitude.height = _estHeight;
    _model.state.altitude.vario = _estVario;

    if(_model.config.debug.mode == DEBUG_ALTITUDE)
    {
      _model.state.debug[0] = std::clamp(lrintf(_model.state.baro.altitudeGround * 100.0f), -32000l, 32000l);  // baro raw cm
      _model.state.debug[1] = std::clamp(lrintf(_model.state.baro.vario * 100.0f), -32000l, 32000l);           // baro vario cm/s
      _model.state.debug[2] = std::clamp(lrintf(_model.state.altitude.height * 100.0f), -32000l, 32000l);      // fused height cm
      _model.state.debug[3] = std::clamp(lrintf(_model.state.altitude.vario * 100.0f), -32000l, 32000l);       // fused vario cm/s
    }

    return 1;
  }

private:
  Model& _model;
  float _estHeight;
  float _estVario;
  float _lastBaroAlt;
  bool _initialized;
};

}
