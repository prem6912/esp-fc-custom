#pragma once

#include "BusDevice.hpp"
#include "Debug_Espfc.h"
#include <algorithm>

#define VL53L0X_ADDRESS_DEFAULT 0x29

namespace Espfc::Device {

class RangefinderVL53L0X
{
public:
  RangefinderVL53L0X(): 
    _bus(nullptr), 
    _addr(VL53L0X_ADDRESS_DEFAULT), 
    _initialized(false), 
    _bufHead(0), 
    _bufCount(0),
    _smoothDist(0.0f),
    _lastReadTime(0),
    _lastValidTime(0),
    _hasValid(false) {}

  int begin(BusDevice* bus, uint8_t addr = VL53L0X_ADDRESS_DEFAULT)
  {
    _bus = bus;
    _addr = addr;
    if (!_bus) return 0;

    // 1. Probe I2C address 0x29
    uint8_t id = 0;
    bool ack = _bus->writeByte(_addr, 0x88, 0x00);
    if (!ack)
    {
      if (_bus->read(_addr, 0xC0, 1, &id) <= 0 && _bus->read(_addr, 0x00, 1, &id) <= 0)
      {
        return 0; // No response on I2C address 0x29
      }
    }

    // 2. STMicroelectronics Standard Tuning Sequence
    writeReg(0x88, 0x00);
    writeReg(0x80, 0x01);
    writeReg(0xFF, 0x01);
    writeReg(0x00, 0x00);
    writeReg(0x91, 0x3C);
    writeReg(0x00, 0x01);
    writeReg(0xFF, 0x00);
    writeReg(0x80, 0x00);

    // 3. Load precision timing & noise threshold registers
    writeReg(0xFF, 0x01);
    writeReg(0x4F, 0x00);
    writeReg(0x4E, 0x2C);
    writeReg(0xFF, 0x00);
    writeReg(0xB6, 0xB4);
    writeReg(0x85, 0x01);
    writeReg(0x86, 0x00);
    writeReg(0x87, 0x00);

    // 4. Set interrupt on new sample ready & clear
    writeReg(0x0A, 0x04); // SYSTEM_INTERRUPT_CONFIG_GPIO
    writeReg(0x84, 0x00); // GPIO_HV_MUX_ACTIVE_HIGH
    writeReg(0x0B, 0x01); // clear interrupt

    // 5. Start Continuous Back-to-Back Ranging Mode
    writeReg(0x00, 0x02);

    _initialized = true;
    _bufCount = 0;
    _hasValid = false;
    _lastReadTime = 0;
    _lastValidTime = 0;
    return 1;
  }

  int readMm(int32_t& distanceMm)
  {
    if (!_initialized || !_bus) return 0;

    uint32_t now = millis();

    // Check every 10ms for conversion completion (33ms ST timing budget)
    if (now - _lastReadTime >= 10)
    {
      _lastReadTime = now;

      // 1. Poll RESULT_INTERRUPT_STATUS_GPIO (0x13) to verify fresh sample is ready
      uint8_t status = 0;
      bool ready = (_bus->read(_addr, 0x13, 1, &status) > 0 && (status & 0x07) != 0);
      
      // Auto-recovery if sensor interrupt was latched
      if (!ready && (now - _lastValidTime > 60))
      {
        ready = true;
      }

      if (ready)
      {
        uint8_t buf[2] = {0, 0};
        int8_t res = _bus->read(_addr, 0x1E, 2, buf);
        
        // 2. Clear interrupt to acknowledge reading and start next continuous measurement
        writeReg(0x0B, 0x01);

        if (res == 2)
        {
          uint16_t rawDist = ((uint16_t)buf[0] << 8) | buf[1];

          // Valid physical distance window: 25mm to 1350mm
          if (rawDist >= 25 && rawDist <= 1350 && rawDist != 8190 && rawDist != 8191)
          {
            _samples[_bufHead] = rawDist;
            _bufHead = (_bufHead + 1) % 5;
            if (_bufCount < 5) _bufCount++;

            // 5-sample Fast Median Filter (eliminates outlier noise with sub-10ms response)
            uint16_t sorted[5];
            for (int i = 0; i < _bufCount; ++i) sorted[i] = _samples[i];
            std::sort(sorted, sorted + _bufCount);
            float medianVal = sorted[_bufCount / 2];

            if (!_hasValid)
            {
              _smoothDist = medianVal;
              _hasValid = true;
            }
            else
            {
              // Fast PT1 Filter: Crisp, smooth millimeter altitude
              _smoothDist = _smoothDist * 0.50f + medianVal * 0.50f;
            }

            _lastValidTime = now;
            distanceMm = (int32_t)(_smoothDist + 0.5f);
            return 1; // Genuine fresh measurement!
          }
        }
      }
    }

    return 0; // Conversion still in progress; allow high-rate inertial prediction
  }

private:
  int writeReg(uint8_t reg, uint8_t val)
  {
    return _bus->writeByte(_addr, reg, val) ? 1 : 0;
  }

  BusDevice* _bus;
  uint8_t _addr;
  bool _initialized;
  uint16_t _samples[7];
  int _bufHead;
  int _bufCount;
  float _smoothDist;
  uint32_t _lastReadTime;
  uint32_t _lastValidTime;
  bool _hasValid;
};

} // namespace Espfc::Device

