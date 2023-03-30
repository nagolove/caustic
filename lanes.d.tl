local record lanes
    configure: function(): lanes
    record Thread
        status: string
        join: function(Thread): any, any
    end
    type func = function(...:any): any
    --gen: function(opts: string, f: func): func
    gen: function(opts: string, f: func): Thread
end

return lanes
