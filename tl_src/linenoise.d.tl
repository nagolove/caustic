local record linenoise
    readline: function(welcome?: string): string
    set_multiline: function(flag: boolean)
    add_history: function(entry: string)
    save_history: function(path: string): boolean
    load_history: function(path: string): boolean
end

return linenoise
