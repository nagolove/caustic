local record sha2
    type Partial = function(message?: string): Partial
    sha256: function(message?: string): Partial|string
    blake3: function(message?: string): Partial|string
end

return sha2
