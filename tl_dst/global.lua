



Modules = {}





Env = {}














Task = {}




ParserSetup = {}






Target = {}





UrlAction = {}






DepLinks = {}

Dependency = {}











































































verbose = false


errexit = false
errexit_uv = true

cmake = {
   ["linux"] = "cmake ",
   ["wasm"] = "emcmake cmake ",

   ['win'] = "cmake ",
}

make = {
   ["linux"] = "make ",
   ["wasm"] = "emmake make ",
   ['win'] = 'make CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ ' ..
   'CFLAGS="-I/usr/x86_64-w64-mingw32/include" ' ..
   'LDFLAGS="-L/usr/x86_64-w64-mingw32/lib" ' ..
   '-D_WIN32 -DWIN32 ',
}


compiler = {
   ['linux'] = 'gcc',
   ['wasm'] = 'emcc',
   ['win'] = 'x86_64-w64-mingw32-gcc',

}

ar = {
   ['linux'] = 'ar',
   ['wasm'] = 'emar',
   ['win'] = 'x86_64-w64-mingw32-ar',
}

























local getenv = os.getenv

local function env_instance()


   local path_rel_third_party = getenv("KOH_MODULE_LINUX") or "modules_linux"
   local path_rel_wasm_third_party = getenv("KOH_MODULE_WASM") or "modules_wasm"
   local path_rel_win_third_party = getenv("KOH_MODULE_WIN") or "modules_windows"
   local path_caustic = os.getenv("CAUSTIC_PATH")

   local cmake_toolchain_win = path_caustic .. "/toolchain-mingw64.cmake"
   local cmake_toolchain_win_opt = "-DCMAKE_TOOLCHAIN_FILE=" .. cmake_toolchain_win

   local e = {
      path_rel_third_party = path_rel_third_party,
      path_rel_wasm_third_party = path_rel_wasm_third_party,
      path_rel_win_third_party = path_rel_win_third_party,

      path_rel_third_party_t = {
         ['linux'] = path_rel_third_party,
         ['wasm'] = path_rel_wasm_third_party,
         ['win'] = path_rel_win_third_party,
      },

      path_abs_third_party = {
         ["linux"] = path_caustic .. "/" .. path_rel_third_party,
         ['wasm'] = path_caustic .. "/" .. path_rel_wasm_third_party,
         ['win'] = path_caustic .. "/" .. path_rel_win_third_party,
      },

      path_caustic = path_caustic,

      cmake_toolchain_win = cmake_toolchain_win,
      cmake_toolchain_win_opt = cmake_toolchain_win_opt,


   }

   return e
end

return {
   env_instance = env_instance,
}
