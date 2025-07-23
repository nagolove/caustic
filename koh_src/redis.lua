#!/usr/bin/env lua

-- Получаем путь к домашней директории и настраиваем пути поиска Lua-модулей,
-- установленных через luarocks, чтобы работали require()
local getenv = os.getenv
local lua_ver = "5.4"
local home = getenv("HOME")

-- Расширяем путь поиска Lua-модулей (*.lua, *.lua/init.lua)
package.path =	home .. "/.luarocks/share/lua/" .. lua_ver .. "/?.lua;" ..
                home .. "/.luarocks/share/lua/" .. lua_ver .. "/?/init.lua;" ..
                package.path

-- Расширяем путь поиска C-модулей (*.so)
package.cpath = home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?.so;" ..
                home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?/init.so;" .. -- init.so — редко используется, но включено на всякий случай
                package.cpath


-- Загружаем Lua-клиент для Redis (установи через luarocks: redis-lua)
local redis = require "redis"

-- Устанавливаем соединение с Redis-сервером по адресу 127.0.0.1:6379
-- Убедись, что redis-server уже запущен!
local client = redis.connect("127.0.0.1", 6379)


-- Пример: сохраняем embedding-вектор (3 числа), как будто это результат нейронной сети
local embedding = {0.01, -0.5, 0.3}

-- Подключаем serpent — сериализатор Lua-таблиц в строки кода
local serpent = require "serpent"

-- Сохраняем сериализованный вектор в Redis с ключом "embedding:1234"
-- Используем обычный string-ключ
client:set("embedding:1234", serpent.dump(embedding))


-- Дополнительно сохраняем метаинформацию о чанке (номер строки, путь и т.д.)
-- Используем Redis-хеш (структура "ключ:поле:значение"), удобная для описания объектов
client:hset("chunk:1234", "path", "src/player.c")
client:hset("chunk:1234", "start", 42)


-- Получаем ранее сохранённый embedding по ключу
local str = client:get("embedding:1234")

-- Компилируем строку Lua-кода обратно в функцию
local f = load(str)

-- Вызываем её, получая оригинальную таблицу-embedding
local vector = f()


-- Подключаем inspect — удобный pretty-printer для отладки Lua-таблиц
local inspect = require 'inspect'

-- Печатаем восстановленный вектор
print(inspect(vector))

