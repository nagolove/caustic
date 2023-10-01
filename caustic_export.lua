-- vim: set tabstop=2
----------------------------------------------------------------------

--[[
Скрипт должен сохранять анимацию в виде, пригодном для чтения средствами
caustic.
Требования:
    - сохранение всех слоев в отдельные файлы формата png
        пример some_file_name_layer_layer_name_00.png
    - сохранение всех кадров анимации каждого слоя
    - [возможно?] создание метафайла описания экспортированного объекта сразу
    в виде lua кода - таблицы
--]]

--print "Hello from script"
local inspect = require "inspect"

--print("_G", inspect(_G))

local logfile = io.open("/tmp/aseprite", "w")
--logfile:write(inspect(_G))

local function dprint(...)
    local args = {...}
    for _, v in ipairs(args) do
        local s = tostring(v)
        if s then
            logfile:write(s .. "\t");
        end
    end
    logfile:write("\n");
end

do
    --[[
    local spr = Sprite(32, 64)

    app.useTool{
    }
    
    spr.properties.k = Uuid()
    app.fs.makeDirectory("~/aseprite")

    spr:saveAs("~/aseprite/very_original_name_1.aseprite") -- works good
    --spr:saveAs(app.fs.fileName("~/aseprite/very_original_name.aseprite"))
    spr:close()
    --]]

    --[[
    local spr = app.activeSprite
    if not spr then return app.alert "There is no active sprite" end

    app.transaction(
    function()
        local bounds = Rectangle()

        logfile:write("bounds: " .. inspect(bounds) .. "\n")

        local layers = app.range.layers
        logfile:write("layers: " .. inspect(layers) .. "\n")

        local newLayer = spr:newLayer()
        newLayer.name = "Glass Floor"
        newLayer.opacity = 128

        for _,frame in ipairs(spr.frames) do
            for _,layer in ipairs(layers) do
                local img = Image(spr.spec)
                for _,l in ipairs(layers) do
                    local layerCel = l:cel(frame)
                    if layerCel then
                        img:drawImage(layerCel.image, layerCel.position)
                        bounds = bounds:union(layerCel.bounds)
                    end
                end

                spr:newCel(newLayer, frame, img, Point(0, 0))
            end
        end

        local oldFrame = app.activeFrame
        app.activeLayer = newLayer
        for _,cel in ipairs(newLayer.cels) do
            app.activeFrame = cel.frame
            app.command.Flip{ target="mask", orientation="vertical" }
            cel.position = Point(cel.position.x,
            cel.position.y + 2*(bounds.y + bounds.height - spr.height/2))
        end
        app.activeFrame = oldFrame
    end)
    --]]

    function export_layers()
        -- Получить все слои
        local active_sprite = app.activeSprite
        dprint("active_sprite", active_sprite)
        if not active_sprite then
            return
        end
        layers = active_sprite.layers
        --dprint("layers", layers)

        -- Цикл по слоям
        for i, layer in ipairs(layers) do
            -- Получить имя файла
            --filename = "layer_example_" .. layer.name .. ".png"
            dprint("layer.name", layer.name)

            -- Экспортировать слой в файл
            --app.exportSprite(filename, layer)
            app.command.ExportSpriteSheet {
                --ui=true,
                askOverwrite=true,
                type=SpriteSheetType.HORIZONTAL,
                columns=0,
                rows=0,
                width=0,
                height=0,
                bestFit=false,
                --textureFilename="",
                textureFilename="easysprite" .. layer.name,
                dataFilename="",
                dataFormat=SpriteSheetDataFormat.JSON_HASH,
                filenameFormat="blaho-{title} ({layer}) {frame}.{extension}",
                borderPadding=0,
                shapePadding=0,
                innerPadding=0,
                trimSprite=false,
                trim=false,
                trimByGrid=false,
                extrude=false,
                --ignoreEmpty=false,
                ignoreEmpty=true,
                mergeDuplicates=false,
                --openGenerated=false,
                openGenerated=true,
                --layer="",
                layer=layer.name,
                tag="",
                splitLayers=false,
                splitTags=false,
                splitGrid=false,
                listLayers=true,
                listTags=true,
                listSlices=true,
                fromTilesets=false,
            }

        end
    end

    --dprint("app.command", app.command)
    -- Вызвать функцию
    export_layers()

end

logfile:close()

