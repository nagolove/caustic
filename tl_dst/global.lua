



Modules = {}





Env = {}



















Task = {}




ParserSetup = {}







Target = {}







targets_all = { "linux", "win", "wasm", "android" }

UrlAction = {}






DepLinks = {}

Dependency = {}



















































































verbose = false


errexit = false
errexit_uv = true





android_api = 24
android_abi = "arm64-v8a"

android_triple = "aarch64-linux-android"




local function android_ndk_find()
   local ndk = os.getenv("ANDROID_NDK_HOME")
   if ndk and ndk ~= "" then
      return ndk
   end
   local sdk = os.getenv("ANDROID_HOME") or "/opt/android-sdk"
   local p = io.popen("ls -1 '" .. sdk .. "/ndk' 2>/dev/null | sort -V | tail -1")
   if not p then
      return ""
   end
   local ver = p:read("*l")
   p:close()
   if not ver or ver == "" then
      return ""
   end
   return sdk .. "/ndk/" .. ver
end

android_ndk = android_ndk_find()

android_bin = android_ndk .. "/toolchains/llvm/prebuilt/linux-x86_64/bin"
local android_cc = android_bin .. "/" .. android_triple .. android_api .. "-clang"

android_cmake_opts = "-DCMAKE_TOOLCHAIN_FILE=" .. android_ndk ..
"/build/cmake/android.toolchain.cmake " ..
"-DANDROID_ABI=" .. android_abi .. " " ..
"-DANDROID_PLATFORM=android-" .. android_api .. " "



cmake = {
   ["linux"] = "cmake ",
   ["wasm"] = "emcmake cmake ",
   ['win'] = 'cmake -DCMAKE_TOOLCHAIN_FILE=' ..
   (os.getenv("CAUSTIC_PATH") or "") ..
   '/toolchain-mingw64.cmake ',
   ['android'] = 'cmake ' .. android_cmake_opts,
}

make = {
   ["linux"] = "make ",
   ["wasm"] = "emmake make ",
   ['win'] = 'make CC=x86_64-w64-mingw32-gcc ' ..
   'CXX=x86_64-w64-mingw32-g++ ' ..
   'AR=x86_64-w64-mingw32-ar ' ..
   'CFLAGS+="-I/usr/x86_64-w64-mingw32/include ' ..
   '-D_WIN32 -DWIN32" ' ..
   'LDFLAGS+="-L/usr/x86_64-w64-mingw32/lib" ',
   ['android'] = 'make CC=' .. android_cc .. ' ' ..
   'CXX=' .. android_cc .. '++ ' ..
   'AR=' .. android_bin .. '/llvm-ar ' ..
   'RANLIB=' .. android_bin .. '/llvm-ranlib ',
}

compiler_c = {
   ['linux'] = 'gcc',
   ['wasm'] = 'emcc',
   ['win'] = 'x86_64-w64-mingw32-gcc',
   ['android'] = android_cc,
}

compiler_cpp = {
   ['linux'] = 'g++',
   ['wasm'] = 'em++',
   ['win'] = 'x86_64-w64-mingw32-g++',
   ['android'] = android_cc .. '++',
}

ar = {
   ['linux'] = 'ar',
   ['wasm'] = 'emar',
   ['win'] = 'x86_64-w64-mingw32-ar',
   ['android'] = android_bin .. '/llvm-ar',
}

























local getenv = os.getenv

local function env_instance()


   local path_rel_third_party = getenv("KOH_MODULE_LINUX") or "modules_linux"
   local path_rel_wasm_third_party = getenv("KOH_MODULE_WASM") or "modules_wasm"
   local path_rel_win_third_party = getenv("KOH_MODULE_WIN") or "modules_windows"
   local path_rel_android_third_party = getenv("KOH_MODULE_ANDROID") or
   "modules_android"
   local path_caustic = os.getenv("CAUSTIC_PATH")

   local cmake_toolchain_win = path_caustic .. "/toolchain-mingw64.cmake"
   local cmake_toolchain_win_opt = "-DCMAKE_TOOLCHAIN_FILE=" .. cmake_toolchain_win

   local e = {
      path_rel_third_party = path_rel_third_party,
      path_rel_wasm_third_party = path_rel_wasm_third_party,
      path_rel_win_third_party = path_rel_win_third_party,
      path_rel_android_third_party = path_rel_android_third_party,

      path_rel_third_party_t = {
         ['linux'] = path_rel_third_party,
         ['wasm'] = path_rel_wasm_third_party,
         ['win'] = path_rel_win_third_party,
         ['android'] = path_rel_android_third_party,
      },

      path_abs_third_party = {
         ["linux"] = path_caustic .. "/" .. path_rel_third_party,
         ['wasm'] = path_caustic .. "/" .. path_rel_wasm_third_party,
         ['win'] = path_caustic .. "/" .. path_rel_win_third_party,
         ['android'] = path_caustic .. "/" .. path_rel_android_third_party,
      },

      path_caustic = path_caustic,

      cmake_toolchain_win = cmake_toolchain_win,
      cmake_toolchain_win_opt = cmake_toolchain_win_opt,

      android_ndk = android_ndk,
      cmake_toolchain_android_opt = android_cmake_opts,


   }

   return e
end

return {
   env_instance = env_instance,
}
