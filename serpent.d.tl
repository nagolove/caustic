--local type Function = function(any): any
local type Serpent = record
    dump: function(a: any, opts: any): string
    line: function(a: any, opts: any): string
    block: function(a: any, opts: any): string
    load: function(string): boolean, any
end
return Serpent
