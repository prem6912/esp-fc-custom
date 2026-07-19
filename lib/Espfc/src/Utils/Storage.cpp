#ifndef UNIT_TEST

#include "Utils/Storage.h"
#include "ModelConfig.h"
#include <Arduino.h>
#include <EEPROM.h>

#if defined(NO_GLOBAL_INSTANCES) || defined(NO_GLOBAL_EEPROM)
static EEPROMClass EEPROM;
#endif

namespace Espfc {

namespace Utils {

int Storage::begin()
{
  EEPROM.begin(EEPROM_SIZE);
  static_assert(sizeof(ModelConfig) <= EEPROM_SIZE, "ModelConfig Size too big");
  return 1;
}

StorageResult Storage::load(ModelConfig& config) const
{
  //return STORAGE_ERR_BAD_MAGIC;

  int addr = 0;
  uint8_t magic = EEPROM.read(addr++);
  if(magic != 0xA5 && magic != 0xA6)
  {
    return STORAGE_ERR_BAD_MAGIC;
  }

  uint8_t version = EEPROM.read(addr++);
  if(EEPROM_VERSION != version)
  {
    return STORAGE_ERR_BAD_VERSION;
  }

  uint16_t size = 0;
  size = EEPROM.read(addr++);
  size |= EEPROM.read(addr++) << 8;

  // If magic is 0xA5 and size is sizeof(ModelConfig), the EEPROM has the scrambled layout.
  // We can descramble it and migrate it to the correct layout!
  if(magic == 0xA5 && size == sizeof(ModelConfig))
  {
    size_t vbat_offset = offsetof(ModelConfig, vbat);
    size_t ibat_offset = offsetof(ModelConfig, ibat);
    size_t cellMin_offset = offsetof(ModelConfig, cellMin);

    uint8_t* ptr = reinterpret_cast<uint8_t*>(&config);

    // 1. Copy part 1 (before vbat)
    for(size_t i = 0; i < vbat_offset; i++)
    {
      ptr[i] = EEPROM.read(addr + i);
    }

    // 2. Read cellMin and cellMax
    int16_t cellMin = EEPROM.read(addr + vbat_offset) | (EEPROM.read(addr + vbat_offset + 1) << 8);
    int16_t cellMax = EEPROM.read(addr + vbat_offset + 2) | (EEPROM.read(addr + vbat_offset + 3) << 8);
    config.cellMin = cellMin;
    config.cellMax = cellMax;

    // 3. Copy part 2 (rest of vbat)
    size_t vbat_rest_size = ibat_offset - vbat_offset;
    for(size_t i = 0; i < vbat_rest_size; i++)
    {
      ptr[vbat_offset + i] = EEPROM.read(addr + vbat_offset + 4 + i);
    }

    // 4. Copy part 3 (everything after vbat up to cellMin)
    size_t rest_size = cellMin_offset - ibat_offset;
    for(size_t i = 0; i < rest_size; i++)
    {
      ptr[ibat_offset + i] = EEPROM.read(addr + ibat_offset + 4 + i);
    }

    return STORAGE_LOAD_SUCCESS;
  }

  if(size != sizeof(ModelConfig) && size != sizeof(ModelConfig) - 4)
  {
    return STORAGE_ERR_BAD_SIZE;
  }

  uint8_t* ptr = reinterpret_cast<uint8_t*>(&config);
  for(uint16_t i = 0; i < size; i++)
  {
    ptr[i] = EEPROM.read(addr + i);
  }
  return STORAGE_LOAD_SUCCESS;
}

StorageResult Storage::save(const ModelConfig& config)
{
  int addr = 0;
  uint16_t size = sizeof(ModelConfig);
  EEPROM.write(addr++, EEPROM_MAGIC);
  EEPROM.write(addr++, EEPROM_VERSION);
  EEPROM.write(addr++, size & 0xFF);
  EEPROM.write(addr++, (size >> 8) & 0xFF);
  EEPROM.put(addr, config);
  bool ok = EEPROM.commit();
  if(!ok) return STORAGE_SAVE_ERROR;
  return STORAGE_SAVE_SUCCESS;
}

}

}

#endif
