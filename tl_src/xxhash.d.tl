local record xxhash
    -- Lua: xxhash64(str) -> raw 8-byte string
    hash64: function(s: string): string

    hash32: function(s: string): string
end

return xxhash
