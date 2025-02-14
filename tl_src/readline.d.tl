
local record rl
    readline: function(prompt: string): string
    save_history: function()
    add_history: function(line: string)
end

return rl
