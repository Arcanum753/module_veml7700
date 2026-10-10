#include "core_web/FSWebServerLib.h"

#include "core_json/core_json.h"
#include "core_state/core_state.h"
#include "core_task/core_task.h"
#include "core_terminal/core_terminal.h"
#include "core_terminal/ErriezSerialTerminal.h"

#include "module_veml7700.h"
#include "common/common.h"
#include "module_veml7700_version.h"
#include "core_sys/eertos.h"

#include <Wire.h>

// Терминальная команда модуля (реализация — в module_veml7700_engine.cpp)
void veml7700CmdLux();

// Bus-функция сохранения (veml7700.save): ставит отложенное сохранение конфига.
static int veml7700BusSave(void* user, int argc, const BusValue* argv, BusValue& result) {
    (void)user; (void)argc; (void)argv; (void)result;
    module_veml7700.saveNow();
    return BUS_OK;
}

CLASS_MODULE_VEML7700 module_veml7700;

CLASS_MODULE_VEML7700::CLASS_MODULE_VEML7700() {
    _fs = NULL;
    _wireStarted = false;
    _available   = false;
    _saturated   = false;
    _lux         = 0.0f;
    _raw         = 0;
    _pendingSave = false;
}

#if defined(ESP32)
void CLASS_MODULE_VEML7700::setFs(fs::LittleFSFS* fs)
#endif
#if defined(ESP8266)
void CLASS_MODULE_VEML7700::setFs(FS* fs)
#endif
{
    _fs = fs;
}

// ============================================================
// begin()
// ============================================================
void CLASS_MODULE_VEML7700::begin() {
    DEBUGVEML7700("%s\r\n", __FUNCTION__);

    defaultConfig();
    if (loadConfig() == false) { saveConfig(); }

    if (!_wireStarted) { _wireStarted = true; Wire.begin(); }

    applyConfig();
    _available = detect() && _config.enabled;

    TerminalRegisterModule(luxTerminalRegister);

    // Периодический опрос через core_task (переарм тем же именем меняет период).
    core_task.every("veml7700_poll", pollTask, _config.pollInterval, true);
}

void CLASS_MODULE_VEML7700::begin(ModContext& ctx) {
    _fs = ctx.fs;
    begin();
}

// ============================================================
// web_Init()
// ============================================================
void CLASS_MODULE_VEML7700::web_Init() {
    DEBUGVEML7700("%s\r\n", __FUNCTION__);

    // AJAX — настройки + статус (инициализация формы)
    ESPHTTPServer.on("/veml7700/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ESPHTTPServer.checkAuth(request)) { return request->requestAuthentication(); }
        this->handleInfo(request);
    });

    // AJAX — только показания (частый опрос без перезаписи формы)
    ESPHTTPServer.on("/veml7700/read", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ESPHTTPServer.checkAuth(request)) { return request->requestAuthentication(); }
        this->handleRead(request);
    });

    // AJAX — сохранение настроек
    ESPHTTPServer.on("/veml7700/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!ESPHTTPServer.checkAuth(request)) { return request->requestAuthentication(); }
        this->handleSave(request);
    });

    // Версия модуля
    ESPHTTPServer.on("/veml7700/ver", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->html_ver_get(request);
    });
}

// ============================================================
// register_resources() — публикация ресурсов на шине core_state
// ============================================================
void CLASS_MODULE_VEML7700::register_resources() {
    DEBUGVEML7700("%s\r\n", __FUNCTION__);

    core_state.regState("lux",       BusValue::F32,  "ambient light (lux)", false);
    core_state.regState("available", BusValue::BOOL, "VEML7700 present on I2C bus", false);

    // Явная функция сохранения конфига (правило apply/save для resource-bus).
    core_state.regFunc("save", "->", "persist /config_veml7700.json", veml7700BusSave, nullptr);
}

// ============================================================
// Веб-обработчики
// ============================================================
void CLASS_MODULE_VEML7700::handleInfo(AsyncWebServerRequest *request) {
    DEBUGVEML7700("%s\r\n", __FUNCTION__);
    String values = "";
    values += "enabled|"      + String(_config.enabled ? "true" : "false") + "|chk\n";
    values += "gain|"         + String(_config.gain)                       + "|select\n";
    values += "it|"           + String(_config.it)                         + "|select\n";
    values += "persistence|"  + String(_config.persistence)                + "|select\n";
    values += "pollInterval|" + String(_config.pollInterval)               + "|input\n";

    char buf[32];
    snprintf(buf, sizeof(buf), "0x%02X", _config.addr);
    values += "veml_addr|" + String(buf) + "|div\n";

    appendStatusCvt(values);
    request->send(200, "text/plain", values);
}

// Общий блок статуса для handleInfo/handleRead (единая точка изменения формата).
void CLASS_MODULE_VEML7700::appendStatusCvt(String &values) {
    values += "veml_available|" + String(_available ? "available" : "not available") + "|div\n";
    values += "veml_lux|" + String(_lux, 3) + " lx|div\n";
    values += "veml_raw|" + String(_raw) + "|div\n";
    values += "veml_sat|" + String(_saturated ? "saturated: reduce gain / integration time" : "") + "|div\n";
}

void CLASS_MODULE_VEML7700::handleRead(AsyncWebServerRequest *request) {
    String values = "";
    appendStatusCvt(values);
    request->send(200, "text/plain", values);
}

void CLASS_MODULE_VEML7700::handleSave(AsyncWebServerRequest *request) {
    DEBUGVEML7700("%s\r\n", __FUNCTION__);

    if (request->args() == 0) {
        request->send(200, "text/plain", "OK");
        return;
    }

    for (uint8_t i = 0; i < request->args(); i++) {
        DEBUGVEML7700("Arg %d: %s %s\r\n", i, request->argName(i).c_str(), request->arg(i).c_str());
        String name = request->argName(i);

        if (name == "enabled") {
            _config.enabled = (request->arg(i) == "true");
            continue;
        }
        if (name == "gain") {
            _config.gain = (uint8_t)constrain(request->arg(i).toInt(), 0, VEML7700_GAIN_COUNT - 1);
            continue;
        }
        if (name == "it") {
            _config.it = (uint8_t)constrain(request->arg(i).toInt(), 0, VEML7700_IT_COUNT - 1);
            continue;
        }
        if (name == "persistence") {
            _config.persistence = (uint8_t)constrain(request->arg(i).toInt(), 0, VEML7700_PERS_COUNT - 1);
            continue;
        }
        if (name == "pollInterval") {
            _config.pollInterval = (uint16_t)constrain(request->arg(i).toInt(),
                                                       VEML7700_POLL_MIN_MS, 60000);
            continue;
        }
    }

    applyConfig();
    _available = detect() && _config.enabled;
    saveConfig();

    // Переармировать опрос с новым периодом.
    core_task.every("veml7700_poll", pollTask, _config.pollInterval, false);

    request->send(200, "text/plain", "OK");
}

// ============================================================
// Конфиг
// ============================================================
void CLASS_MODULE_VEML7700::defaultConfig() {
    _config.addr         = VEML7700_I2C_ADDR_DEFAULT;
    _config.enabled      = true;
    _config.gain         = 0;   // x1
    _config.it           = 2;   // 100 мс
    _config.persistence  = 0;   // 1
    _config.pollInterval = VEML7700_POLL_DEFAULT_MS;
}

bool CLASS_MODULE_VEML7700::loadConfig() {
    DEBUGVEML7700("%s\r\n", __FUNCTION__);
    JsonDocument doc;
    if (core_json.jsonFileLoadDoc(CONFIG_FILE_VEML7700, doc) == false) { return false; }

    _config.addr         = (uint8_t)doc["addr"].as<unsigned int>();
    _config.enabled      = doc["enabled"].as<bool>();
    _config.gain         = (uint8_t)doc["gain"].as<unsigned int>();
    _config.it           = (uint8_t)doc["it"].as<unsigned int>();
    _config.persistence  = (uint8_t)doc["persistence"].as<unsigned int>();
    _config.pollInterval = doc["pollInterval"].as<uint16_t>();

    // Защита от некорректных значений в файле.
    if (_config.addr == 0) { _config.addr = VEML7700_I2C_ADDR_DEFAULT; }
    if (_config.gain >= VEML7700_GAIN_COUNT) { _config.gain = 0; }
    if (_config.it >= VEML7700_IT_COUNT) { _config.it = 2; }
    if (_config.persistence >= VEML7700_PERS_COUNT) { _config.persistence = 0; }
    if (_config.pollInterval < VEML7700_POLL_MIN_MS) { _config.pollInterval = VEML7700_POLL_DEFAULT_MS; }

    DEBUGVEML7700("addr: 0x%02X, enabled: %d, gain: %d, it: %d, pers: %d, poll: %d\r\n",
                  _config.addr, _config.enabled, _config.gain, _config.it,
                  _config.persistence, _config.pollInterval);
    return true;
}

bool CLASS_MODULE_VEML7700::saveConfig() {
    DEBUGVEML7700("%s\r\n", __FUNCTION__);
    JsonDocument doc;
    core_json.jsonFileLoadDoc(CONFIG_FILE_VEML7700, doc);

    doc["addr"]         = _config.addr;
    doc["enabled"]      = _config.enabled;
    doc["gain"]         = _config.gain;
    doc["it"]           = _config.it;
    doc["persistence"]  = _config.persistence;
    doc["pollInterval"] = _config.pollInterval;

    return core_json.jsonFileSaveDoc(CONFIG_FILE_VEML7700, doc);
}

// ============================================================
// Публичное API (часть, зависящая только от конфига)
// ============================================================
bool CLASS_MODULE_VEML7700::isAvailable() const { return _available; }
float CLASS_MODULE_VEML7700::getLux() const { return _lux; }
uint16_t CLASS_MODULE_VEML7700::getRaw() const { return _raw; }
bool CLASS_MODULE_VEML7700::isSaturated() const { return _saturated; }
uint8_t CLASS_MODULE_VEML7700::getAddr() const { return _config.addr; }
uint16_t CLASS_MODULE_VEML7700::getPollIntervalMs() const { return _config.pollInterval; }

void CLASS_MODULE_VEML7700::saveNow() {
    _pendingSave = true;
    SetTask(deferredSaveTask);
}

// ============================================================
// Версионные методы
// ============================================================
String CLASS_MODULE_VEML7700::getVersionStr() {
    return String(MODULE_VEML7700_VERSION);
}

String CLASS_MODULE_VEML7700::getGeneratedTime() {
    return String(MODULE_VEML7700_GENERATED_TIME);
}

String CLASS_MODULE_VEML7700::getCommitDateStr() {
    return String(MODULE_VEML7700_COMMIT_DATE_STR);
}

void CLASS_MODULE_VEML7700::html_ver_get(AsyncWebServerRequest *request) {
    DEBUGVEML7700("%s\r\n", __FUNCTION__);
    String values = "";
    values += "vemlversion|"  + getVersionStr()    + "|div\n";
    values += "vemlgentime|"  + getGeneratedTime() + "|div\n";
    values += "vemlgendate|"  + getCommitDateStr() + "|div\n";
    request->send(200, "text/plain", values);
}

// ============================================================
// Терминальные команды
// ============================================================
void luxTerminalRegister() {
    term.addCommand("lux", veml7700CmdLux);
}
