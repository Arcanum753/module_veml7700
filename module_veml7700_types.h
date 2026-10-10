#ifndef _MODULE_VEML7700_TYPES_h
#define _MODULE_VEML7700_TYPES_h

// ============================================================
// module_veml7700_types.h — типы, структуры и define'ы модуля VEML7700.
// Реализация: module_veml7700.cpp (шаблон) и module_veml7700_engine.cpp
// (I2C, пересчёт raw→lux, периодический опрос, терминал).
// ============================================================

#include <Arduino.h>
#include <stdint.h>

#define CONFIG_FILE_VEML7700        "/config_veml7700.json"

// Адрес VEML7700 на шине I2C
#define VEML7700_I2C_ADDR_DEFAULT   0x10

// Регистры VEML7700 (16-бит, little-endian)
#define VEML7700_REG_ALS_CONF       0x00   // ALS_CONF_0
#define VEML7700_REG_ALS_WH         0x01
#define VEML7700_REG_ALS_WL         0x02
#define VEML7700_REG_POWER_SAVE     0x03
#define VEML7700_REG_ALS            0x04   // сырое значение датчика
#define VEML7700_REG_WHITE          0x05
#define VEML7700_REG_ALS_INT        0x06

// Битовые поля ALS_CONF_0 (позиции сверить с даташитом Vishay)
#define VEML7700_CONF_SD_BIT        0      // ALS_SD: 0 = enabled, 1 = shutdown
#define VEML7700_CONF_INT_EN_BIT    1      // ALS_INT_EN
#define VEML7700_CONF_PERS_SHIFT    4      // ALS_PERS [1:0]
#define VEML7700_CONF_IT_SHIFT      6      // ALS_IT [3:0]
#define VEML7700_CONF_GAIN_SHIFT    11     // ALS_GAIN [1:0]

// Количество поддерживаемых значений параметров
#define VEML7700_GAIN_COUNT         4      // x1, x2, x1/4, x1/8
#define VEML7700_IT_COUNT           6      // 25/50/100/200/400/800 мс
#define VEML7700_PERS_COUNT         4      // 1/2/4/8

// Порог «насыщения» сырого значения ALS (около 16-битного максимума)
#define VEML7700_RAW_SATURATED      60000

// Период периодического опроса, мс
#define VEML7700_POLL_MIN_MS        200
#define VEML7700_POLL_DEFAULT_MS    1000

// ============================================================
// Конфиг — сохраняется в /config_veml7700.json
// ============================================================
typedef struct {
    uint8_t  addr;          // I2C-адрес (по умолчанию 0x10)
    bool     enabled;       // true — ALS_SD=0 (датчик включён)
    uint8_t  gain;          // индекс 0..3 (x1, x2, x1/4, x1/8)
    uint8_t  it;            // индекс 0..5 (25/50/100/200/400/800 мс)
    uint8_t  persistence;   // индекс 0..3 (1/2/4/8)
    uint16_t pollInterval;  // период опроса, мс
} strVeml7700Config;

#endif // _MODULE_VEML7700_TYPES_h
