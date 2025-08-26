local record sha2
    type partial = function(message?: string): partial
    sha256: function(message?: string): partial|string
    blake3: function(message?: string): partial|string
end

return sha2
