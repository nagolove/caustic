#!/usr/bin/env lua
--#!/usr/bin/env lua5.4

local getenv = os.getenv
local lua_ver = "5.4"
local home = getenv("HOME")
package.path =	home .. "/.luarocks/share/lua/" .. lua_ver .. "/?.lua;" ..
                home .. "/.luarocks/share/lua/" .. lua_ver .. "/?/init.lua;" .. package.path
package.cpath = home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?.so;" ..
        home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?/init.so;" .. -- XXX: init.so?
        package.cpath

local http = require("socket.http")
local ltn12 = require("ltn12")
local json = require("dkjson")  -- либо cjson, если у тебя другой JSON-модуль
local inspect = require "inspect"

local Assist = {}
Assist.__index = Assist

function Assist.new()
    local self = {}
    self.timers = {}
    return setmetatable(self, Assist)
end

function Assist:send(str) 
    local body = json.encode({
        model = "google/gemma-3-12b",  -- id из /v1/models
        messages = {
            { role = "system", content = "Ты LLM-ассистент сборочной системы, который отвечает строго в JSON-командах." },
            { role = "user", content = "Кто ты?" },
            { role = "user", content = str }
        }
    })

    local response_body = {}
    local res, code, headers = http.request{
        url = "http://localhost:1234/v1/chat/completions",  -- ✅ OpenAI-совместимый API
        method = "POST",
        headers = {
            ["Content-Type"] = "application/json",
            ["Content-Length"] = tostring(#body)
        },
        source = ltn12.source.string(body),
        sink = ltn12.sink.table(response_body)
    }

    if code == 200 then
        local response_json = table.concat(response_body)
        local decoded = json.decode(response_json)
        return (decoded.choices[1].message.content)
    else
        print("HTTP error:", code)
        print(table.concat(response_body))
    end

    return ""
end

function Assist:models_list()
    local response_body = {}

    local res, code, headers = http.request{
        url = "http://localhost:1234/v1/models",
        method = "GET",
        sink = ltn12.sink.table(response_body)
    }

    if res and code == 200 then
        local body = table.concat(response_body)
        local data = json.decode(body)
        return (inspect(data))
    else
        print("HTTP error:", code)
    end

    return ""
end

local a = Assist.new()
print(a:models_list())
print(a:send("Какая инфомация тебе доступна?"))
