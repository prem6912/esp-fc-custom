#include "VoltageSensor.h"

#include <algorithm>

namespace Espfc {

namespace Sensor {

VoltageSensor::VoltageSensor(Model &model) : _model(model) {}

int VoltageSensor::begin()
{
  _model.state.battery.timer.setRate(100);
  _model.state.battery.samples = 50;

  // 50Hz effective sampling rate per channel (VBAT/IBAT alternating)
  _vFilterFast.begin(FilterConfig(FILTER_PT1, 10), 50);
  _vFilter.begin(FilterConfig(FILTER_PT2, 1), 50);

  _iFilterFast.begin(FilterConfig(FILTER_PT1, 10), 50);
  _iFilter.begin(FilterConfig(FILTER_PT2, 1), 50);

  _state = VBAT;

  return 1;
}

int VoltageSensor::update()
{
  if (!_model.state.battery.timer.check()) return 0;

  Utils::Stats::Measure measure(_model.state.stats, COUNTER_BATTERY);

  switch (_state)
  {
  case VBAT:
    _state = IBAT;
    return readVbat();
  case IBAT:
    _state = VBAT;
    return readIbat();
  }

  return 0;
}

int VoltageSensor::readVbat()
{
#ifdef ESPFC_ADC_0
  if (_model.config.vbat.source != 1 || _model.config.pin[PIN_INPUT_ADC_0] == -1) return 0;

  // 16-sample oversampling to eliminate ESP32 ADC thermal noise & WiFi RF transmission bursts
  uint32_t adcSum = 0;
  int pin = _model.config.pin[PIN_INPUT_ADC_0];
  for (int i = 0; i < 16; ++i)
  {
    adcSum += analogRead(pin);
  }
  _model.state.battery.rawVoltage = adcSum / 16;

  float volts = _vFilterFast.update((float)_model.state.battery.rawVoltage * ESPFC_ADC_SCALE);

  volts *= _model.config.vbat.scale * 0.1f;
  volts *= _model.config.vbat.resMult;
  volts /= _model.config.vbat.resDiv;

  _model.state.battery.voltageUnfiltered = volts;
  _model.state.battery.voltage = _vFilter.update(_model.state.battery.voltageUnfiltered);

  // cell count detection
  if (_model.state.battery.samples > 0)
  {
    _model.state.battery.cells = std::ceil(_model.state.battery.voltage / (_model.config.cellMax * 0.01f));
    _model.state.battery.samples--;
  }

  _model.state.battery.cellVoltage = _model.state.battery.voltage / constrain(_model.state.battery.cells, 1, 6);
  _model.state.battery.percentage = Utils::clamp(Utils::map(_model.state.battery.cellVoltage, _model.config.cellMin * 0.01f, _model.config.cellMax * 0.01f, 0.0f, 100.0f), 0.0f, 100.0f);

  if (_model.config.debug.mode == DEBUG_BATTERY)
  {
    _model.state.debug[0] = constrain(lrintf(_model.state.battery.voltageUnfiltered * 100.0f), 0, 32000);
    _model.state.debug[1] = constrain(lrintf(_model.state.battery.voltage * 100.0f), 0, 32000);
  }
  return 1;
#else
  return 0;
#endif
}

int VoltageSensor::readIbat()
{
#ifdef ESPFC_ADC_1
  if (_model.config.ibat.source != 1 || _model.config.pin[PIN_INPUT_ADC_1] == -1) return 0;

  uint32_t adcSum = 0;
  int pin = _model.config.pin[PIN_INPUT_ADC_1];
  for (int i = 0; i < 16; ++i)
  {
    adcSum += analogRead(pin);
  }
  _model.state.battery.rawCurrent = adcSum / 16;

  float volts = _iFilterFast.update((float)_model.state.battery.rawCurrent * ESPFC_ADC_SCALE);
  float milivolts = volts * 1000.0f;

  volts += _model.config.ibat.offset * 0.001f;
  volts *= _model.config.ibat.scale * 0.1f;

  _model.state.battery.currentUnfiltered = volts;
  _model.state.battery.current = _iFilter.update(_model.state.battery.currentUnfiltered);

  if (_model.config.debug.mode == DEBUG_CURRENT_SENSOR)
  {
    _model.state.debug[0] = lrintf(milivolts);
    _model.state.debug[1] = constrain(lrintf(_model.state.battery.currentUnfiltered * 100.0f), 0, 32000);
    _model.state.debug[2] = _model.state.battery.rawCurrent;
  }

  return 1;
#else
  return 0;
#endif
}

}

}
