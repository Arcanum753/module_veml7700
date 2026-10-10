#ifndef _MODULE_VEML7700_h
#define _MODULE_VEML7700_h

#include "main.h"

#include "mod_context.h"

#ifdef DEBUG_VEML7700
#define DEBUGVEML7700(...) DBG_MOD("[M_VEML7700] ", __VA_ARGS__)
#else
#define DEBUGVEML7700(...)
#endif

#if defined(ESP32)
#include <LittleFS.h>
#endif

#if defined(ESP8266)
#include <LittleFS.h>
#endif

#include "module_veml7700_types.h"

// ============================================================
// module_veml7700 — люксметр VEML7700 (I2C, 0x10).
// Публикует на шину только lux и доступность датчика; настройки — через web.
// ============================================================
class CLASS_MODULE_VEML7700 {
public:
    CLASS_MODULE_VEML7700();
#if defined(ESP32)
    void setFs(fs::LittleFSFS* fs);
#endif
#if defined(ESP8266)
    void setFs(FS* fs);
#endif
    void begin();
    void begin(ModContext& ctx);
    void web_Init();
    void register_resources();

    // ===== Публичное API =====
    void pollStep();                 // один цикл опроса датчика и публикации на шину
    void saveNow();                  // отложенное сохранение конфига (bus-функция save)

    bool     isAvailable() const;
    float    getLux() const;
    uint16_t getRaw() const;
    bool     isSaturated() const;
    uint8_t  getAddr() const;
    uint16_t getPollIntervalMs() const;
    uint16_t getItMs() const;
    const char* getGainName() const;

private:
    // Версионные методы
    String getVersionStr();
    String getGeneratedTime();
    String getCommitDateStr();
    void html_ver_get(AsyncWebServerRequest *request);

    // Веб-обработчики
    void handleInfo(AsyncWebServerRequest *request);
    void handleRead(AsyncWebServerRequest *request);
    void handleSave(AsyncWebServerRequest *request);
    void appendStatusCvt(String &values);

    // Конфиг
    void defaultConfig();
    bool loadConfig();
    bool saveConfig();

    // I2C (реализация — в module_veml7700_engine.cpp)
    bool detect();
    bool readReg(uint8_t reg, uint16_t &val);
    bool writeReg(uint8_t reg, uint16_t val);
    void applyConfig();

    // EERTOS-задачи
    static void pollTask();
    static void deferredSaveTask();

protected:
#if defined(ESP32)
    fs::LittleFSFS* _fs;
#endif
#if defined(ESP8266)
    FS*             _fs;
#endif

    strVeml7700Config _config;
    bool     _wireStarted;
    bool     _available;
    bool     _saturated;
    float    _lux;
    uint16_t _raw;
    bool     _pendingSave;
};

extern CLASS_MODULE_VEML7700 module_veml7700;

// Терминальные команды модуля (регистрируются через TerminalRegisterModule)
void luxTerminalRegister();

#endif // _MODULE_VEML7700_h
