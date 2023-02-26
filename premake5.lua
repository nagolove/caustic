local sanit = false
local caustic = require 'caustic'

local tabular = require 'tabular'
print(tabular(caustic))
load(caustic.premake5_code)

--[[
workspace "caustic"
    configurations { "Debug", "Release" }

    defines{"GRAPHICS_API_OPENGL_43"}

    includedirs(caustic.includedirs)
    includedirs({ "src" })

    if sanit then
        linkoptions {
            "-fsanitize=address",
            "-fsanitize-recover=address",
        }
        buildoptions { 
            "-fsanitize=address",
            "-fsanitize-recover=address",
        }
    end

    buildoptions {
        "-ggdb3",
        "-fPIC",
        "-Wall",
        --"-Wno-strict-aliasing",
    }

    links(caustic.links_internal)
    libdirs(caustic.libdirs_internal)

    language "C"
    --cppdialect "C++20"
    kind "ConsoleApp"
    targetprefix ""
    targetdir "."
    symbols "On"

    project "libcaustic"
        kind "StaticLib"
        if sanit then
            linkoptions {
                "-fsanitize=address",
                "-fsanitize-recover=address",
            }
        end
        buildoptions { 
            "-ggdb3",
        }
        files { 
            "./src/*.h", 
            "./src/*.c",
        }
        removefiles("*main.c")

    --[[
    project "test_objects_pool"
        buildoptions { 
            "-ggdb3",
        }
        files {
            "src/*.h",
            "src/object.c",
            "tests/munit.c",
            "tests/test_object_pool.c",
        }

    project "test_de_ecs"
        buildoptions { 
            "-ggdb3",
        }
        files {
            "tests/munit.c",
            "tests/test_de_ecs.c",
        }

    project "test_timers"
        buildoptions { 
            "-ggdb3",
        }
        files {
            "src/*.h",
            "src/timer.c",
            "tests/munit.c",
            "tests/test_timers.c",
        }

    project "test_array"
        buildoptions { 
            "-ggdb3",
        }
        files {
            "src/*.h",
            "src/array.c",
            "tests/munit.c",
            "tests/test_array.c",
        }

    project "test_console"
        buildoptions { 
            "-ggdb3",
        }
        files {
            "src/*.h",
            "src/object.c",
            "tests/munit.c",
            "tests/test_console.c",
        }

    project "test_table"
        buildoptions { 
            "-ggdb3",
        }
        files {
            "src/table.c",
            "tests/munit.c",
            "tests/test_table.c",
        }
    
        
    project "test_strset"
        buildoptions { 
            "-ggdb3",
        }
        files {
            "src/strset.c",
            "tests/munit.c",
            "tests/test_strset.c",
        }
    --]]
    --
    --[[
    
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        buildoptions { 
            "-ggdb3"
        }
        linkoptions {
            "-fsanitize=address",
        }
        buildoptions { 
            "-fsanitize=address",
        }

    filter "configurations:Release"
        --buildoptions { 
            --"-O2"
        --}
        --symbols "On"
        --defines { "NDEBUG" }
        --optimize "On"

]]
