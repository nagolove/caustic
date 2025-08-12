-- posix/signal.d.tl

local type SigHandler = function(integer)

local record M
   SIGINT: integer

   -- Минимальная сигнатура: номер сигнала + функция-обработчик.
   -- Возврат задаём как any, чтобы не спорить с реальной библиотекой.
   signal: function(signum: integer, handler: SigHandler): any
end

return M

