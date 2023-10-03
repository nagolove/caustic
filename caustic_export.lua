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

-- Экспортировать слой в файл
local function export(filename)
    app.command.ExportSpriteSheet{
        ui=false,
        askOverwrite=false,
        type=SpriteSheetType.HORIZONTAL,
        columns=0,
        rows=0,
        width=0,
        height=0,
        bestFit=false,
        textureFilename=filename,
        dataFilename="",
        dataFormat=SpriteSheetDataFormat.JSON_HASH,
        borderPadding=0,
        shapePadding=0,
        innerPadding=0,
        trim=false,
        layerIndex = i,
        ----mergeDuplicates=data.mergeDuplicates,
        mergeDuplicates=false,
        extrude=false,
        openGenerated=false,
        layer="",
        tag="",
        splitLayers=false,
        --listLayers=layer,
        listLayers=true,
        listTags=true,
        listSlices=true,
    }
end

local spr = app.activeSprite

if Sprite == nil then
   -- Show error, no sprite active.
   local dlg = MsgDialog("Error", "No sprite is currently active. Please, open a sprite first and run again.")
   dlg:show()
   return 1
end

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

function export_layers(output_path)
    -- Получить все слои
    local active_sprite = app.activeSprite
    dprint("active_sprite", active_sprite)
    if not active_sprite then
        return
    end
    layers = active_sprite.layers
    --dprint("layers", layers)
   
    local layers_visibility = {}

    -- Цикл по слоям
    -- Запоминание видимости
    for i, layer in ipairs(layers) do
        layers_visibility[i] = layer.isVisible
    end

    -- Все слои невидимы
    for i, layer in ipairs(layers) do
        layer.isVisible = false
    end

    -- Цикл по слоям
    for i, layer in ipairs(layers) do
        -- Получить имя файла
        --filename = "layer_example_" .. layer.name .. ".png"
        dprint("layer.name", layer.name)

        local filename =    output_path .. 
                            app.fs.fileTitle(app.activeSprite.filename) .. 
                            "_" ..
                            layer.name .. 
                            ".png"
        layer.isVisible = true
        export(filename)
        layer.isVisible = false
    end

    -- Восстановление видимости
    for i, layer in ipairs(layers) do
        layer.isVisible = layers_visibility[i]
    end
end

-- Open main dialog.
local dlg = Dialog("Export slices")
dlg:file{
    id = "directory",
    label = "Output directory:",
    --filename = app.activeSprite.filename,
    open = false
}
--[[
dlg:entry{
    id = "filename",
    label = "File name format:",
    text = "{slicedata}" .. "." .. "{slicename}"
}
dlg:combobox{
    id = 'format',
    label = 'Export Format:',
    option = 'png',
    options = {'png', 'gif', 'jpg'}
}
dlg:slider{id = 'scale', label = 'Export Scale:', min = 1, max = 10, value = 1}
dlg:check{id = "save", label = "Save sprite:", selected = false}
dlg:button{id = "ok", text = "Export"}
dlg:button{id = "cancel", text = "Cancel"}
--]]
dlg:button{id = "ok", text = "Export"}
dlg:button{id = "cancel", text = "Cancel"}
dlg:show()

if not dlg.data.ok then 
    return 0 
end

--dprint("app.command", app.command)
-- Вызвать функцию

local separator = string.sub(app.activeSprite.filename, 1, 1) == "/" and "/" or "\\"

function dirname(str)
   return str:match("(.*" .. separator .. ")")
end

--local filename = dlg.data.filename .. "." .. dlg.data.format
local output_path = dirname(dlg.data.directory)
dprint('output_path', output_path)
export_layers(output_path)

logfile:close()

