local record Zip
    record file
        uncompressed_size: integer
        filename: string
    end
    type FileIterator = function(): file
    record zfile
        files: function(zfile): FileIterator
        open: function(zfile, string): FILE
    end
    open: function(fname: string): zfile, string
end

return Zip
