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
http.TIMEOUT = 235  -- Время в секундах
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

--function Assist:send(str) 
local function test_send()
    local body = json.encode({
        model = "google/gemma-3-12b",  -- id из /v1/models
        --model = "meta-llama-3-8b-instruct",
        --model = "deepseek/deepseek-r1-0528-qwen3-8b",
        messages = {
            --{ role = "system", content = "Ты LLM-ассистент сборочной системы игрового движка, который отвечает строго в JSON-командах." },
            { role = "system", content = "Ты LLM-ассистент сборочной системы игрового движка, который отвечает строго в Lua-таблицах." },
            { role = "user", content = "Кто ты?" },
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

function models_list()
    local response_body = {}

    local res, code, headers = http.request{
        url = "http://localhost:1234/v1/models",
        method = "GET",
        sink = ltn12.sink.table(response_body)
    }

    if res and code == 200 then
        local body = table.concat(response_body)
        local data = json.decode(body)

        local clean_data = {}
        if data and data.data and type(data.data) == 'table' then
            for k, v in ipairs(data.data) do
                if v.object and v.object == 'model' then
                    table.insert(clean_data, v.id)
                end
            end
        end

        return clean_data
    else
        print("HTTP error:", code)
    end

    return {}
end

--[[
local a = Assist.new()
print(a:models_list())
print(a:send("Какая инфомация тебе доступна?"))
--]]


local cURL = require("cURL")
local json = require("dkjson")  -- или другой модуль JSON

function send2llm(payload_table, on_get_data_chunk)
    local json_payload = json.encode(payload_table)
    local response = {}

    local easy = cURL.easy{
        url = "http://localhost:1234/v1/chat/completions",
        post = true,
        httpheader = {
            "Content-Type: application/json",
            "Content-Length: " .. tostring(#json_payload)
        },
        postfields = json_payload,
        timeout = 60 * 10,        -- Таймаут в секундах (общий)
        connecttimeout = 30, -- Таймаут подключения
        writefunction = function(chunk)

            --table.insert(response, data)
            --return #data

            for line in chunk:gmatch("([^\n]*)\n?") do
                if line:match("^data:") then
                    local content = line:match("^data:%s*(.*)")
                    if content == "[DONE]" then
                        return
                    end

                    local ok, data = pcall(json.decode, content)
                    if ok and data and data.choices and data.choices[1] then
                        local delta = data.choices[1].delta
                        if delta and delta.content then

                            --io.write(delta.content)  -- печатай по мере получения
                            if type(on_get_data_chunk) == 'function' then
                                on_get_data_chunk(delta.content)
                            end
                            table.insert(response, delta.content)

                        end
                    end
                end
            end
            return #chunk


        end
    }

    easy:perform()
    easy:close()

    return table.concat(response)
end

function embedding(modelname, text)
    local payload = json.encode({
        input = text,
        model = modelname,
    })

    --print('embedding', inspect(payload))

    local result = {}

    local easy = cURL.easy{
        url = "http://localhost:1234/v1/embeddings",
        post = true,
        httpheader = {
            "Content-Type: application/json",
            "Content-Length: " .. #payload
        },
        postfields = payload,
        timeout = 60 * 10,        -- Таймаут в секундах (общий)
        connecttimeout = 30, -- Таймаут подключения
        writefunction = function(chunk)
            table.insert(result, chunk)
            return #chunk
        end
    }

    easy:perform()
    easy:close()

    local full_response = table.concat(result)
    local response = json.decode(full_response)

    --print('response', inspect(response))

    return response.data[1].embedding  -- массив чисел (vector)
    --return {}
end


--[[
-- Пример вызова
local result = send2llm {
    --model = "local-llm",
    model = "google/gemma-3-12b",  -- id из /v1/models
    messages = {
        { role = "system", content = "Ты ассистент..." },
        --{ role = "user", content = "Сколько будет 2+2?" }
        { role = "user", content = "Расскажи о Льве Николаевиче Толстом. Чем он занимался когда ему было 20 лет?" }
    },
    stream = true,
    --max_tokens = 2048,
}
--]]

--print(result)

return {
    models_list = models_list,
    send2llm = send2llm,
    embedding = embedding,
    test_send = test_send,
}
--]]
