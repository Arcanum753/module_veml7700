-- veml7700_brightness.lua
-- Авто-яркость матрицы «Электроника-7» RGB по освещённости VEML7700.
--   lux < 20         -> e7.brightness = 5
--   20 <= lux <= 100 -> e7.brightness = 20
--   lux > 100        -> e7.brightness = 35  (максимум)
--
-- Условия объединяются по AND; каждое правило срабатывает по фронту false->true,
-- поэтому яркость переключается при переходе между диапазонами.
-- Запись e7.brightness разрешена только в режиме e7.mode = macro, поэтому первым
-- телом макрос сам переводит устройство в этот режим (один раз при старте).
-- Правила выполняются только когда датчик доступен (veml7700.available == true).
return {
    desc = "VEML7700 lux -> e7.brightness (5/20/35)",
    rules = {
        { set = "e7.mode", value = "macro" },

        { when = { cond = {
            { res = "veml7700.available", op = "==", val = true },
            { res = "veml7700.lux",       op = "<",  val = 20 },
        } }, set = "e7.brightness", value = 5 },

        { when = { cond = {
            { res = "veml7700.available", op = "==", val = true },
            { res = "veml7700.lux",       op = ">=", val = 20 },
            { res = "veml7700.lux",       op = "<=", val = 100 },
        } }, set = "e7.brightness", value = 20 },

        { when = { cond = {
            { res = "veml7700.available", op = "==", val = true },
            { res = "veml7700.lux",       op = ">",  val = 100 },
        } }, set = "e7.brightness", value = 35 },
    },
}
