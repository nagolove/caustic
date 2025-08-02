local inspect = require "inspect"
local tabular = require 'tabular'

-- chunk_size   размер чанка в строках
-- start_from   номер строки, с которой начинается сбор
-- gap          количество строк зазора между чанками. 
--              если == 0, то обработка идет последовательно
local function make_chunker(id, chunk_size, start_from, gap)
    assert(chunk_size > 0)
    assert(start_from >= 0)
    assert(gap >= 0)

    --print("init")

    local chunks, chunk = {}, {}
    local sz = 0          -- размер текущего чанка
    local cur_gap = gap   -- текущий размер пробела между чанками

    local coro = coroutine.create(function()
        --print(id, 'start_from', start_from)

        for j = 1, start_from do
            local inp = coroutine.yield()
            --print(id, "declined", inp, "j", j)
        end

        while true do

            sz = chunk_size
            while true do
                local inp = coroutine.yield()
                --print(id, "inp", inp)
                sz = sz - 1
                table.insert(chunk, inp)
                if sz <= 0 then
                    break
                end
            end

            table.insert(chunks, chunk)
            chunk = {}

            for _ = 1, gap, 1 do
                local inp = coroutine.yield()
                --print(id, "gap", inp)
            end

        end

    end)

    -- Съедаю строку что-бы началь заполнение таблиц с первого элемента, а не
    -- второго
    coroutine.resume(coro, "")

    return {
        step = function(line)
            assert(line)
            coroutine.resume(coro, line)
        end,
        stop = function()
            --print(id, 'stop')

            if #chunk ~= 0 then
                table.insert(chunks, chunk)
            end

            return chunks
        end,
    }
end

local function split2chunks(fname, chunk_size, overlap)
    assert(#fname > 0)
    assert(chunk_size > 0)
    assert(overlap >= 0)
    assert(chunk_size > overlap)
    --local overlap = 0
    --local chunk_size = 10
    local gap = chunk_size - overlap * 2
    local m1, m2 = make_chunker("id1", chunk_size, 0, gap),
                   make_chunker("id2", chunk_size, chunk_size - overlap, gap)

    local f = io.open(fname, "r")
    for line in f:lines() do
        m1.step(line)
        m2.step(line)
    end

    --[[
    print(inspect(m1.stop()))
    print ""
    print(inspect(m2.stop()))
    print ""
    --]]
    
    local final_chunks = {}
    for _, ch in ipairs(m1.stop()) do
        table.insert(final_chunks, ch)
    end
    for _, ch in ipairs(m2.stop()) do
        table.insert(final_chunks, ch)
    end
    return final_chunks
end

local final_chunks = chunk_monster("chunks.txt", 400, 4)
print("final_chunks", inspect(final_chunks))
--print(tabular(final_chunks))

