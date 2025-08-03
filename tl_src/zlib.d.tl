local record zlib
   -- Constants (optional, you can expand as needed)
   BEST_SPEED: integer
   BEST_COMPRESSION: integer
   DEFAULT_COMPRESSION: integer

   -- Version info
   version: function(): (integer, integer, integer | nil)

   -- Streaming deflate
   deflate: function(compression_level: integer | nil, window_size: integer | nil): function(string, string | nil): (string, boolean, integer, integer)

   -- Streaming inflate
   inflate: function(window_size: integer | nil): function(string): (string, boolean, integer, integer)

   -- Checksum factories
   adler32: function(): function(string): integer
   crc32: function(): function(string): integer
end

return zlib

