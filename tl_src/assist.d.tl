
local record Assist
    new: function(): Assist
    send: function(Assist, query: string): string
    models_list: function(Assist): {string}
end

return Assist
