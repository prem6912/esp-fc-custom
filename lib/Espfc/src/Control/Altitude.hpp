#pragma once

#include "Model.h"
#include "Utils/Filter.h"
#include "Device/RangefinderVL53L0X.hpp"
#include <algorithm>
#include <cmath>

namespace Espfc::Control {

class Altitude
{
public:
  Altitude(Model& model): 
    _model(model), 
    _estHeight(0.0f), 
    _estVario(0.0f), 
    _accBias(0.0f), 
    _prevRawLaser(0.0f),
    _lastLaserSampleTime(0),
    _dt(0.0f) {}

  int begin()
  {
    _model.state.altitude.height = 0.0f;
    _model.state.altitude.vario = 0.0f;

    _dt = 1.0f / (float)_model.state.accel.timer.rate;
    _estHeight = 0.0f;
    _estVario = 0.0f;
    _accBias = 0.0f;
    _prevRawLaser = 0.0f;
    _lastLaserSampleTime = 0;

    // Initialize VL53L0X Rangefinder directly on shared I2C bus (obtained via Gyro device)
    Device::BusDevice* bus = nullptr;
    if (_model.state.gyro.present && _model.state.gyro.dev)
    {
      bus = _model.state.gyro.dev->getBusDev();
    }
    else if (_model.state.baro.present && _model.state.baro.dev)
    {
      bus = _model.state.baro.dev->getBusDev();
    }

    if (bus && _rangefinder.begin(bus))
    {
      _model.state.rangefinder.present = true;
      _model.logger.info().log(F("RANGE")).logln("VL53L0X OK");
    }

    return 1;
  }

  // Core 1: I2C hardware acquisition
  int read()
  {
    if (!_model.state.rangefinder.present) return 0;
    int32_t distMm = 0;
    if (_rangefinder.readMm(distMm))
    {
      float rawDistM = (float)distMm * 0.001f;
      _model.state.rangefinder.distance = rawDistM;
      _model.state.rangefinder.valid = (rawDistM >= 0.03f && rawDistM <= 1.30f);
      _model.state.rangefinder.fresh = true;
      _model.state.rangefinder.lastSampleTime = millis();
      return 1;
    }
    return 0;
  }

  // Core 1: Mathematical sensor fusion
  int update()
  {
    Utils::Stats::Measure measure(_model.state.stats, COUNTER_IMU_FUSION2);

    // 1. High-rate 500Hz Inertial Prediction
    float accZ = _model.state.accel.world.z;
    float accCorr = accZ - _accBias;

    _estHeight += _estVario * _dt + 0.5f * accCorr * _dt * _dt;
    _estVario  += accCorr * _dt;

    // 2. Discrete Innovation Correction strictly on fresh 30Hz laser packets
    if (_model.state.rangefinder.fresh)
    {
      _model.state.rangefinder.fresh = false;

      float cosTilt = std::max(_model.state.attitude.cosTheta, 0.6f);
      float laserVertical = _model.state.rangefinder.distance * cosTilt;
      bool laserValid = _model.state.rangefinder.valid && (laserVertical >= 0.03f && laserVertical <= 1.25f);

      if (laserValid)
      {
        uint32_t now = millis();
        float dtSample = (now - _lastLaserSampleTime) * 0.001f;

        // Derive physical vertical velocity from consecutive 30Hz laser measurements
        if (_lastLaserSampleTime > 0 && dtSample >= 0.010f && dtSample <= 0.150f)
        {
          float laserVel = (laserVertical - _prevRawLaser) / dtSample;
          laserVel = std::clamp(laserVel, -1.50f, 1.50f);
          _estVario += 0.30f * (laserVel - _estVario); // Physical velocity measurement tracking
        }

        _prevRawLaser = laserVertical;
        _lastLaserSampleTime = now;

        // Zero-lag position tracking + complementary error damping
        float error = laserVertical - _estHeight;
        error = std::clamp(error, -0.40f, 0.40f);
        _estHeight += 0.45f * error;
        _estVario  += 0.10f * error; // Belt-and-braces position innovation damping

        if (_model.isModeActive(MODE_ARMED))
        {
          _accBias -= 0.005f * error;
          _accBias = std::clamp(_accBias, -0.50f, 0.50f);
        }
        else
        {
          _accBias = 0.0f;
        }
      }
    }

    // 3. Sensor loss / occlusion timeout (100ms watchdog)
    uint32_t now = millis();
    if (now - _model.state.rangefinder.lastSampleTime > 100)
    {
      _estVario *= (1.0f - _dt * 0.25f); // gentle damping
      if (!_model.isModeActive(MODE_ARMED))
      {
        _estHeight = 0.0f;
        _estVario = 0.0f;
        _accBias = 0.0f;
      }
    }

    _model.state.altitude.height = _estHeight;
    _model.state.altitude.vario = _estVario;
    return 1;
  }

private:
  Model& _model;
  Device::RangefinderVL53L0X _rangefinder;
  float _estHeight;
  float _estVario;
  float _accBias;
  float _prevRawLaser;
  uint32_t _lastLaserSampleTime;
  float _dt;
};

} // namespace Espfc::Control



