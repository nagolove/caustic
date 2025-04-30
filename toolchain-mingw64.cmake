set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Компиляторы
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

# Компоновщик
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Указываем пути к стандартным библиотекам MinGW
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Не искать заголовки и библиотеки в системе Linux
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

