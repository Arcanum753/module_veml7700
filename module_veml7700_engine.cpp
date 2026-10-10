#include "core_web/FSWebServerLib.h"

#include "core_state/core_state.h"
#include "core_sys/eertos.h"
#include "core_terminal/core_terminal.h"
#include "core_terminal/ErriezSerialTerminal.h"

#include "module_veml7700.h"

#include <Wire.h>

// ============================================================
// Таблицы параметров VEML7700
// ============================================================

// ALS_IT: индекс 0..5 -> код поля [3:0] (мс)
static const uint8_t  kVemlItCode[VEML7700_IT_COUNT] = { 0x0C, 0x08, 0x00, 0x01, 0x02, 0x03 };
static const uint16_t kVemlItMs[VEML7700_IT_COUNT]   = { 25, 50, 100, 200, 400, 800 };

// ALS_GAIN: индекс 0..3 -> код поля [1:0]
// порядок имён: x1, x2, x1/4, x1/8 (коды 00, 01, 11, 10)
static const uint8_t kVemlGainBits[VEML7700_GAIN_COUNT] = { 0x00, 0x01, 0x03, 0x02 };
static const char* const kVemlGainNames[VEML7700_GAIN_COUNT] = { "x1", "x2", "x1/4", "x1/8" };

// Разрешение, lx/count: [gain][it]
static const float kVemlResolution[VEML7700_GAIN_COUNT][VEML7700_IT_COUNT] = {
    { 0.2304f, 0.1152f, 0.0576f, 0.0288f, 0.0144f, 0.0072f }, // x1
    { 0.1152f, 0.0576f, 0.0288f, 0.0144f, 0.0072f, 0.0036f }, // x2
    { 0.9216f, 0.4608f, 0.2304f, 0.1152f, 0.0576f, 0.0288f }, // x1/4
    { 1.8432f, 0.9216f, 0.4608f, 0.2304f, 0.1152f, 0.0576f }, // x1/8
};

// ============================================================
// I2C — низкоуровневые операции
// ============================================================

bool CLASS_MODULE_VEML7700::detect() {
    if (!_wireStarted) { _wireStarted = true; Wire.begin(); }
    Wire.beginTransmission(_config.addr);
    return (Wire.endTransmission() == 0);
}

bool CLASS_MODULE_VEML7700::writeReg(uint8_t reg, uint16_t val) {
    if (!_wireStarted) { _wireStarted = true; Wire.begin(); }
    Wire.beginTransmission(_config.addr);
    Wire.write(reg);
    Wire.write((uint8_t)(val & 0xFF));
    Wire.write((uint8_t)((val >> 8) & 0xFF));
    return (Wire.endTransmission() == 0);
}

bool CLASS_MODULE_VEML7700::readReg(uint8_t reg, uint16_t &val) {
    if (!_wireStarted) { _wireStarted = true; Wire.begin(); }
    Wire.beginTransmission(_config.addr);
    Wire.write(reg);
    // Repeated START (без STOP) — протокол чтения VEML7700.
    if (Wire.endTransmission(false) != 0) { return false; }
    if (Wire.requestFrom((int)_config.addr, 2) != 2) { return false; }
    if (Wire.available() < 2) { return false; }
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    val = (uint16_t)lo | ((uint16_t)hi << 8);
    return true;
}

// Собрать и записать ALS_CONF_0 из конфига.
void CLASS_MODULE_VEML7700::applyConfig() {
    uint16_t conf = 0;
    conf |= ((uint16_t)kVemlGainBits[_config.gain] << VEML7700_CONF_GAIN_SHIFT);
    conf |= ((uint16_t)kVemlItCode[_config.it]     << VEML7700_CONF_IT_SHIFT);
    conf |= ((uint16_t)_config.persistence         << VEML7700_CONF_PERS_SHIFT);
    if (!_config.enabled) { conf |= (1u << VEML7700_CONF_SD_BIT); }
    writeReg(VEML7700_REG_ALS_CONF, conf);
    DEBUGVEML7700("ALS_CONF=0x%04X\r\n", conf);
}

// ============================================================
// Опрос датчика и публикация на шину
// ============================================================

void CLASS_MODULE_VEML7700::pollStep() {
    if (!_wireStarted) { _wireStarted = true; Wire.begin(); }

    if (!detect()) {
        _available = false;
        _saturated = false;
        _raw = 0;
        _lux = 0.0f;
        core_state.signal("veml7700.available", BusValue::bo(false));
        core_state.signal("veml7700.lux", BusValue::f32(0.0f));
        return;
    }

    // Значения доступны макросам, только если датчик включён (enabled).
    // Иначе available=false, чтобы lux=0 не трактовался как реальные 0 лк.
    if (!_config.enabled) {
        _available = false;
        _saturated = false;
        _raw = 0;
        _lux = 0.0f;
        core_state.signal("veml7700.available", BusValue::bo(false));
        core_state.signal("veml7700.lux", BusValue::f32(0.0f));
        return;
    }

    uint16_t raw = 0;
    if (!readReg(VEML7700_REG_ALS, raw)) {
        _available = false;
        _saturated = false;
        _raw = 0;
        _lux = 0.0f;
        core_state.signal("veml7700.available", BusValue::bo(false));
        core_state.signal("veml7700.lux", BusValue::f32(0.0f));
        return;
    }

    _available = true;
    _raw = raw;
    _saturated = (raw >= VEML7700_RAW_SATURATED);
    _lux = (float)raw * kVemlResolution[_config.gain][_config.it];
    core_state.signal("veml7700.available", BusValue::bo(true));
    core_state.signal("veml7700.lux", BusValue::f32(_lux));
}

void CLASS_MODULE_VEML7700::pollTask() {
    module_veml7700.pollStep();
}

void CLASS_MODULE_VEML7700::deferredSaveTask() {
    if (!module_veml7700._pendingSave) { return; }
    module_veml7700._pendingSave = false;
    module_veml7700.saveConfig();
}

// ============================================================
// Геттеры параметров
// ============================================================

uint16_t CLASS_MODULE_VEML7700::getItMs() const {
    if (_config.it >= VEML7700_IT_COUNT) { return 0; }
    return kVemlItMs[_config.it];
}

const char* CLASS_MODULE_VEML7700::getGainName() const {
    if (_config.gain >= VEML7700_GAIN_COUNT) { return "?"; }
    return kVemlGainNames[_config.gain];
}

// ============================================================
// Терминальная команда lux
// ============================================================

void veml7700CmdLux() {
    Serial.printf("\r\nVEML7700 @0x%02X: %s\r\n",
                  module_veml7700.getAddr(),
                  module_veml7700.isAvailable() ? "available" : "not available");
    if (!module_veml7700.isAvailable()) { return; }

    Serial.printf("Lux: %.3f lx  (raw=%u, gain=%s, IT=%ums)\r\n",
                  module_veml7700.getLux(),
                  module_veml7700.getRaw(),
                  module_veml7700.getGainName(),
                  module_veml7700.getItMs());
    if (module_veml7700.isSaturated()) {
        Serial.printf("Warning: saturated — reduce gain / integration time\r\n");
    }
}
