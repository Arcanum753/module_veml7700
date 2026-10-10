# module_veml7700

Опциональный модуль для [avr-fota](https://github.com/Arcanum753/avr-fota): драйвер
датчика освещённости **Vishay VEML7700** (I2C, адрес `0x10`) для ESP8266/ESP32.
Читает яркость, показывает её на веб-странице и в терминале, публикует значение на
ресурсную шину ядра (доступно движку макросов).

## Возможности

- Чтение яркости в **люксах** (регистр `ALS`, пересчёт raw→lux по таблице разрешения
  gain × интеграция).
- Настройка датчика: `enabled` (shutdown), усиление (x1/x2/x1/4/x1/8), время интеграции
  (25/50/100/200/400/800 мс), persistence, период опроса.
- Веб-страница `veml7700.html`: показания в лк (автообновление) и настройки, сохранение
  без перезагрузки.
- Терминальная команда `lux`: значение в лк, доступность, raw и параметры датчика.
- Публикация на шину `core_state` для движка макросов: `veml7700.lux` (F32),
  `veml7700.available` (BOOL) и функция `veml7700.save`.
- Пример макроса `web/macros/veml7700_brightness.lua` — управление яркостью матрицы
  «Электроника-7» RGB по освещённости (`<20 → 5`, `20..100 → 20`, `>100 → 35`).

## Подключение

Модуль — отдельный репозиторий; клонируется в `src/module_veml7700/` ядра avr-fota.
Флаг `MODULE_VEML7700` выводится из `src_filter` автоматически.

```ini
[env:esp32-veml7700]
extends = env:esp32
src_filter = ${platformio.src_filter} +<module_veml7700/> +<module_udp/> +<module_otaclient/>

[registry]
object    = module_veml7700
define    = MODULE_VEML7700
namespace = veml7700
web = 1
loop = 0
res = 1
```

Env: `esp32-veml7700`, `esp8266-veml7700`. I2C — общая шина `Wire` (выводы по умолчанию:
ESP32 21/22, ESP8266 4/5).

## Ресурсы шины (namespace `veml7700`)

| Имя | Тип | RW | Смысл |
|-----|-----|----|-------|
| `veml7700.lux` | F32 | ro | освещённость, лк |
| `veml7700.available` | BOOL | ro | датчик найден **и** включён |
| `veml7700.save` | — | — | сохранить `/config_veml7700.json` |

`veml7700.available = false` (и `lux = 0`), если датчик отсутствует или выключен —
чтобы макросы не принимали `0` за реальную освещённость.

## Веб-интерфейс

| Метод | URL | Назначение |
|---|---|---|
| GET | `/veml7700/info` | настройки + статус |
| GET | `/veml7700/read` | только показания (частый опрос) |
| POST | `/veml7700/save` | сохранить настройки |
| GET | `/veml7700/ver` | версия модуля |

## Конфиг

`/config_veml7700.json`:
```json
{"addr":16,"enabled":true,"gain":0,"it":2,"persistence":0,"pollInterval":1000}
```

## Терминал

```
lux   — текущее значение в лк, доступность, raw и параметры
```

## Структура

```
module_veml7700_types.h    — define'ы, карта регистров, структура конфига
module_veml7700.h          — класс CLASS_MODULE_VEML7700
module_veml7700.cpp        — шаблонный блок (begin/web_Init/ресурсы/конфиг/версии)
module_veml7700_engine.cpp — I2C, пересчёт raw→lux, опрос, терминал
module_veml7700.ini        — env-примеры + [registry]
web/                       — veml7700.html, _menu.html, config_veml7700.json, macros/
```

## Документация

- Конвенции ядра: `TRS.md`, `LAYERS.md`, `BUILD.md` в репозитории avr-fota.
- Подробности компонента: [`AGENTS.md`](AGENTS.md).

## Примечание

Точные позиции бит `ALS_PERS`/`ALS_IT`/`ALS_GAIN` и таблица разрешения — по документации
Vishay VEML7700; при доработке сверить с даташитом. Пороги (`ALS_WH`/`ALS_WL`) и
прерывание (`ALS_INT_EN`) не используются.
