
local record hnswlib

    -- это будет userdata с установленной метатаблицей
    record Handle
        -- hash_id - строка длины 4, хранит xxhash32 от текста чанка
        add: function(Handle, vector: {number}, hash_id: string)
        search: function(Handle, vector: {number}, k: integer): {integer}
        search_str: function(Handle, vector: {number}, k: integer): {string}
        save: function(Handle, fname: string): boolean
    end

    new: function(max_elements: integer, M: integer, ef_construction: integer): Handle
    load: function(fname: string): Handle
end

return hnswlib
