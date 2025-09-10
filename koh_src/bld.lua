-- vim: set colorcolumn=85
-- vim: fdm=marker

return {

   -- еденица трансляции
   {
      -- список отклченных для данной сборки зависимостей
      not_dependencies = {
          "llama_cpp",
          "xxhash",
          "freetype",
          "wormseffect",
          "nanosvg",
          --"munit",
          "uthash",
          "enkits",
          "pcre2",
          "imgui",
          "rlimgui",
          "raylib",
          "cimgui",
          "box2c",
          "chipmunk",
          "lua",
          "sol2",
          "utf8proc",
          "stb",
          "wfc",
          "libtess2",
      },
      -- результат компиляции и линковки
      artifact = "libkoh.so",
      kind = 'shared',
      --kind = "app",
      --kind = "shared",
      --kind = "static",
      -- файл с функцикй main()
      main = "koh.cpp",

      -- каталог с исходными текстами
      --src = "src",
      
      -- исключать следующие имена файлов, можно использовать Lua шаблоны
      exclude = {
            --"t80_stage_empty.c",
      },
      -- список дефайнов которые применяются всегда
      common_define = {
      },
      -- список дефайнов для отладочной сборки
      debug_define = {
         ["BOX2C_SENSOR_SLEEP"] = "1",
        --DEBUG = 0,
      },
      -- список дефайнов для релизной сборки
      release_define = {
         ["T80_NO_ERROR_HANDLING"] = "1",
      },
      -- куда подставляются флаги?
      flags = {
         --"-fopenmp",
      },
      --[[
      codegen = {
         {
            -- нет входного или выходного файла - нет кодогенерации
            --file_in = "t80_defaults.c.in",
            --file_out = "t80_defaults.c",
            on_read = function(line)
            end,
            on_write = function(capture)
                return {
                    "line1",
                    "line2",
                }
            end,
            on_finish = function()
            end,
         },
      },
      --]]
   },
   -- конец еденицы трансляции

}
