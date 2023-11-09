local record posix
    record unistd
        exec: function(string, {string}): string, integer
    end
    type SigHandler = function(signum: integer)
    record signal
        signal: function(signum: integer, handler: SigHandler)
    end
end

return posix
