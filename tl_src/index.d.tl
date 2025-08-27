local record index
    record Index
        chunks_num: function(Index): integer
        chunk_raw: function(Index, i: integer): string
    end
    new: function(fname: string): Index
end

return index
