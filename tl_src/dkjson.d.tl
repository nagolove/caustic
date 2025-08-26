
--[[
- local type json = {string:json}
]]

local record dkjson
    record JsonState
        indent: boolean
        keyorder: {string}
        level: number
        buffer: {string}
        bufferlen: number
        tables: {table:boolean}
        exception: function(string, string, string, string): boolean|string, string
    end
    encode: function({string:any}, ?JsonState): string

    --decode: function(string, number, any, table): {string:any}

    -- Возврат:
    -- 1) value: any         -- разобранное Lua-значение (таблица/строка/число/булево/`nullval`)
    -- 2) pos: integer       -- позиция в строке, где парсинг завершился
    -- 3) err: string?       -- сообщение об ошибке, если парсинг не удался
    -- Если парсинг удачен: возвращаются (value, pos)
    -- Если ошибка: возвращается (nil, pos, err)

    decode: function(str: string): {string:any}, integer, string
    --decode: function(string): {string:any}

    null: table

    version: string

    quotestring: function(string): string

    addnewline: function(JsonState)

    encodeexception: function(string, any, JsonState, string): string

    use_lpeg: function(): dkjson
end

return dkjson
