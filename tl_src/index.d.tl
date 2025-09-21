local record index
    record Index
        chunks_num: function(Index): integer
        chunk_raw: function(Index, i: integer): string

        chunk_text: function(Index, i: integer): string
        chunk_text_zlib: function(Index, i: integer): string
        chunk_embedding: function(Index, i: integer): string
        chunk_id: function(Index, i: integer): string
        chunk_id_hash: function(Index, i: integer): string
        chunk_file: function(Index, i: integer): string
    end
    new: function(fname: string): Index
end

return index
