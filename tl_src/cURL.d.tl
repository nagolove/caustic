local record curl
    record PerformTable
        writefunction: function(str: string)
    end
    record Context
        setopt_url: function(Context, url:string)
        perform: function(Context, PerformTable)
    end
    easy_init: function(): Context
end

return curl
