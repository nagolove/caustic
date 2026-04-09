-- Архив закомментированных команд и вспомогательных функций
-- Вынесено из caustic.tl для облегчения чтения

-- {{{ Старые dependency-функции

--[[
local function build_with_cmake_w(dep: Dependency)
    print('build_with_cmake_w: current dir', lfs.currentdir())
    print('build_with_cmake_w: dep', inspect(dep))
    cmd_do("emcmake cmake .")
    cmd_do("emmake make")
end
--]]

--[[
local function sunvox_after_init()
    print('sunvox_after_init:', lfs.currentdir())
    cmd_do("cp sunvox/sunvox_lib/js/lib/sunvox.wasm sunvox/sunvox_lib/js/lib/sunvox.o")
end
--]]

--[[
local function gennann_after_build(dep: Dependency)
    print('linking genann to static library', dep.dir)
    ut.push_current_dir()
    print("dep.dir", dep.dir)
    chdir(dep.dir)
    cmd_do("ar rcs libgenann.a genann.o")
    ut.pop_dir()
end
--]]

--[[
local function build_cimgui_w(dep: Dependency)
    print('build_cimgui:', inspect(dep))

    cmd_do("cp ../rlImGui/imgui_impl_raylib.h .")

    print("current dir", lfs.currentdir())
    cmd_do("emmake make clean")
    cmd_do("emmake make CFLAGS=\"-g3\"")
end
--]]

--[[
local function freetype_after_init(_: Dependency)
    cmd_do({
        "git submodule update --init --force --recursive --depth 1",
        "cmake -E remove CMakeCache.txt",
        "cmake -E remove_directory CMakeFiles",
        "cmake -E make_directory build",
        "cmake -E chdir build cmake ..",
    })

    push_current_dir()
    chdir("build")
    cmd_do("make -j")
    pop_dir()
end
--]]

--[[
local function build_lfs(_: Dependency)
    print('lfs_custom_build', lfs.currentdir())
    cmd_do("gcc -c src/lfs.c -I/usr/include/lua5.1")
    cmd_do("ar rcs liblfs.a lfs.o")
end
--]]

--[[
local function build_rlwr_common(dep: Dependency)
    print("build_rlwr_common:", lfs.currentdir(), dep.target)
    cmd_do("sh build.sh target=" .. dep.target)
end
--]]

--[[
local function defines_luafun(_: Dependency): {string}
end
--]]

--[[
local function lualibrary_install_luafun(
    abs_project_root_dir: string, to_dir: string
)
    ut.push_current_dir()
    chdir(to_dir)
    mkdir('luafun')
    ut.pop_dir()
    print('lualibrary_install_luafun:', lfs.currentdir())
    print('lualibrary_install_luafun:', to_dir)
    cmd_do(format("cp fun.lua %s/luafun", to_dir))

    chdir(abs_project_root_dir)
    chdir("src")

    local header = io.open("package_path.h", "w")
    header:close()

    local source = io.open("package_path.h", "w")
    source:close()
end
--]]

--[[
local function build_resvg(_: Dependency)
    print('build_resvg')
    ut.push_current_dir()
    cmd_do("cargo build --release")
    chdir("crates/c-api")
    cmd_do("cargo build --release")
    ut.pop_dir()
end
--]]

--[[
local function update_default(_: Dependency)
    print("update_default")
    print("current directory", lfs.currentdir())
    local cmd = "git pull origin master"
    cmd_do(cmd)
end
--]]

--[[
local function build_remotery(_: Dependency)
    print('build_remotery', lfs.currentdir())
    ut.push_current_dir()
    chdir("lib")
    cmd_do("gcc -c Remotery.c  -I lib -pthread -lm")
    cmd_do("ar -rcs  \"libremotery.a\" Remotery.o")
    ut.pop_dir()
end
--]]

--[[
local function wait_threads(threads: {lanes.Thread})
    local waiting = true
    while waiting do
        waiting = false
        for _, thread in ipairs(threads) do
            if thread.status == 'running' then
                waiting = true
                break
            end
        end
        sleep(0.01 as integer)
    end
end
--]]

-- }}}

-- {{{ Закомментированные actions-команды

-- parser_setup записи для закомментированных команд:
--[[
    mmap = {
        summary = [[testing mmap library for chunks.index.copy]]
    },
    sha256 = {
        summary = [[testing sha2.lua]],
    },
    embeddings = {
        summary = [[make embeddings vectors for tags.json]]
    },
    xxhash = {
        summary = [[testing xxhash32 and xxhash64 in lua]]
    },
    chunks_open = {
        summary = [[function for testing chunks splitting system]]
    },
    ai = {
        summary = [[connect to ai assist]]
    },
    load_index = {
        summary = [[load index from binary file to hnsw lib instance]],
    },
    files_koh = {
        summary = [[get list of caustic files for chunking = modules + src]],
    },
    updates_check = {
        summary = "print new avaible sources versions for all dependencies",
        options = { "-n --name", },
    },
--]]

-- {{{ Вспомогательные типы и функции для команд

--[[
local enum ChunkEntropy
    "simple"
    "medium"
    "complex"
end
--]]

--[[
local record Chunker
    step: function(line: string)
    stop: function(): {Chunk}
end
--]]

--[[
-- id           строка для вывода в отладку, идентификатор объекта
-- chunk_size   размер чанка в строках
-- start_from   номер строки, с которой начинается сбор
-- gap          количество строк зазора между чанками.
--              если == 0, то обработка идет последовательно
local function make_chunker(
    _: string, chunk_size: integer, start_from: integer, gap: integer
): Chunker
    assert(chunk_size > 0)
    assert(start_from >= 0)
    assert(gap >= 0)

    local chunks, chunk: {Chunk}, {string} = {}, {}
    local sz = 0          -- размер текущего чанка
    local i = 0           -- сквозная нумерация строк

    local coro = coroutine.create(function()
        for _ = 1, start_from do
            coroutine.yield()
            i = i + 1
        end

        while true do
            sz = chunk_size
            while true do
                local inp = coroutine.yield() as string
                i = i + 1
                sz = sz - 1
                table.insert(chunk, inp)
                if sz <= 0 then
                    break
                end
            end

            table.insert(chunks, {
                text = concat(chunk, "\n"),
                line_start = i,
                line_end = i + chunk_size,
            })
            chunk = {}

            for _ = 1, gap, 1 do
                coroutine.yield()
                i = i + 1
            end
        end

    end)

    coroutine.resume(coro, "")

    return {
        step = function(line)
            assert(line)
            coroutine.resume(coro, line)
        end,
        stop = function(): {Chunk}
            if #chunk ~= 0 then
                table.insert(chunks, {
                    text = concat(chunk, "\n"),
                    line_start = i,
                    line_end = i + #chunk,
                })
            end
            return chunks
        end,
    }
end
--]]


--[[
local function split2chunks(
    fname: string, chunk_size: integer, overlap: integer
): {Chunk}
    assert(#fname > 0)
    assert(chunk_size > 0)
    assert(overlap >= 0)
    assert(chunk_size > overlap)
    local gap = chunk_size - overlap * 2
    local m1, m2 = make_chunker("id1", chunk_size, 0, gap),
                   make_chunker("id2", chunk_size, chunk_size - overlap, gap)

    local f = io.open(fname, "r")
    for line in f:lines() do
        m1.step(line)
        m2.step(line)
    end

    local final_chunks = {}
    for _, ch in ipairs(m1.stop()) do
        table.insert(final_chunks, ch)
    end
    for _, ch in ipairs(m2.stop()) do
        table.insert(final_chunks, ch)
    end
    return final_chunks
end
--]]

--[[
local function format_size(bytes: integer): string
    if bytes < 1024 then
        return format("%d B", bytes)
    elseif bytes < 1024 * 1024 then
        return format("%.2f KB", bytes / 1024)
    else
        return format("%.2f MB", bytes / (1024 * 1024))
    end
end
--]]

--[[
local function load_chunks2string(fname: string): string
    local window_bits = 15
    local stream = zlib.inflate(window_bits)

    local infile = assert(io.open(fname, "rb"))
    local chunk_size = 1024 * 128
    local eof = false
    local decompressed = {}

    while not eof do
        local chunk = infile:read(chunk_size)
        if not chunk then break end

        local out, finished = stream(chunk)
        table.insert(decompressed, out or "")

        if finished then
            eof = true
        end
    end

    infile:close()
    return concat(decompressed)
end
--]]

--[[
local function bin_to_hex(str: string): string
    return (str:gsub('.', function(byte: string): string
        return string.format('%02X', string.byte(byte))
    end))
end
--]]

--[[
local record IndexSetup
    files: {string}
    zlib_level: integer
    zlib_window: integer
    index_fname: string
end
--]]

--[[
local function index_create(index_setup: IndexSetup)
    assert(index_setup)

    local level = index_setup.zlib_level or 3
    local window = index_setup.zlib_window or 15
    local deflate = zlib.deflate(level, window)

    local attr_index = lfs.attributes(index_setup.index_fname)
    if attr_index then
        print(format("index_create: file '%s' exists, aborting"))
        os.exit(1)
    end

    local files = index_setup.files
    assert(type(files) == 'table')

    local dest_zlib_fname = "chunks_koh_tmp.zlib"
    local f = io.open(dest_zlib_fname, "w")
    assert(f)

    local chunks: {Chunk} = {}
    for _, fname in ipairs(files) do
        local attr = lfs.attributes(fname)

        if not attr then
            goto _continue
        end

        if attr.mode ~= 'file' then
            goto _continue
        end

        printc(
            "%{yellow}fname " .. fname .. "%{reset} " ..
            "%{blue}size " .. format_size(ceil(attr.size)) .. "%{reset}"
        )

        local chunks_tmp = split2chunks(
            fname, chunk_lines_num, chunk_lines_overlap
        )

        for _, chunk in ipairs(chunks_tmp) do
            chunk.file = fname
            chunk.id = fname .. ":" .. chunk.line_start
        end

        for _, ch in ipairs(chunks_tmp) do
            table.insert(chunks, ch)
        end

        ::_continue::

    end

    process_chunks_embedding(chunks)

    local dump = serpent.dump(chunks)
    local compressed, eof_1, bytes_in, bytes_out = deflate(dump, nil)
    print("eof", eof_1, "bytes_in", bytes_in, "bytes_out", bytes_out)
    f:write(compressed)

    local final, eof_2, _, _ = deflate("", "finish")
    print("eof", eof_2)
    f:write(final)

end
--]]

--[[
local record Searcher
    query: function(q: string): {string}
end

local function searcher_instance(index_fname: string): Searcher
    local attr_index = lfs.attributes(index_fname)
    if not attr_index then
        print(format(
            "searcher_instance: could not open file '%s', aborting", index_fname
        ))
        os.exit(1)
    end

    local hash2chunk: {string: Chunk} = {}

    local dim = 1500
    local ef = 50
    local searcher = hnswlib.new(100000, dim, ef)

    local Index = require "index"
    local index = Index.new(index_fname)

    local t_start = os.clock()

    for i = 1, index:chunks_num() do
        local ok, errmsg = pcall(function()
            local s: string = index:chunk_raw(i)
            local chunk: Chunk = load(s)() as Chunk
            hash2chunk[chunk.id_hash] = chunk

            searcher:add(chunk.embedding as {number}, chunk.id_hash)
        end) as boolean, string
        if not ok then
            print('errmsg', inspect(errmsg))
        end
    end

    local t_finish = os.clock()
    print("searcher_instance: searcher loaded for", t_finish - t_start, "sec")

    return {
        query = function(q: string): {string}
            local emb = assist.embedding(llm_embedding_model, q)

            assert(emb[1])
            assert(#emb[1] == llm_embedding_dim)
            local nearest = searcher:search_str(emb[1], 10)

            local texts: {string} = {}

            for _, id_hash in ipairs(nearest) do
                local ch = hash2chunk[id_hash]
                if ch then
                    local text = concat({
                        "// file: " .. ch.file .. " , " ..
                        "line_start " .. ch.line_start .. " , " ..
                        "line_end " .. ch.line_end .. "\n\n",
                    })
                    text = text .. ch.text
                    table.insert(texts, text)
                end
            end

            return texts
        end,
    }
end
--]]

--[[
local record Markdown
    feed: function(inp: string): string
end

local function markdown_instance(): Markdown
    linenoise.save_history("history.txt")
    local in_code = false

    local markdown_f = io.open("markdown.txt", "a")
    assert(markdown_f)

    local tok_buf: {string} = {}
    local tok_buf_size = 5
    for i = 1, tok_buf_size do
        tok_buf[i] = ""
    end
    local tok_i = 1

    return {
        feed = function(_inp: string): string
            tok_i = (tok_i + 1) % tok_buf_size
            tok_buf[tok_i] = _inp

            local inp = concat(tok_buf)

            markdown_f:write("inp:", inp .. "\n")

            local out = ""

            if in_code then
                out = "\27[7m" .. _inp .. "\27[0m"
            else
                out = _inp
            end

            if not in_code then
                local c, _ = string.match(inp, "```")
                if c then
                    in_code = true

                    if in_code then
                        local C = ansicolors
                        out = C("%{greenbg}") .. _inp .. C("%{reset}")
                    else
                        out = _inp
                    end
                end
            else
                local c = string.match(inp, "```")
                if c then
                    in_code = false
                end
            end

            markdown_f:flush()
            return out
        end,
    }
end
--]]

-- }}}

-- {{{ Закомментированные actions

--[[
function actions.xxhash(_: Args)
    local str = "Hello, mister Den!"
    print('str', str)

    print("xxhash32", hash32(str))
    print("xxhash64", hash64(str))
end
--]]

--[[
function actions.mmap(_: Args)
    local Index = require "index"
    local index = Index.new("./chunks.bin")
    for i = 1, index:chunks_num() do
        local ok, errmsg = pcall(function()
            local s: string = index:chunk_raw(i)
            local chunk: Chunk = load(s)() as Chunk
            chunk.embedding = nil
            printc("%{blue}" .. inspect(chunk) .. "%{reset}")

            local text = index:chunk_text(i)
            local text_zlib = index:chunk_text_zlib(i)
            local embedding = index:chunk_embedding(i)
            local chunk_id = index:chunk_id(i)
            local chunk_id_hash = index:chunk_id_hash(i)
            local chunk_file = index:chunk_file(i)

            print(
                "text", text,
                "text_zlib", text_zlib,
                "embedding", embedding,
                "chunk_id", chunk_id,
                "chunk_id_hash", chunk_id_hash,
                "chunk_file", chunk_file
            )

        end) as boolean, string
        if not ok then
            print('errmsg', inspect(errmsg))
        end

    end
end
--]]

--[[
function actions.sha256(_: Args)
    local f = io.open("chunks.zlib")
    assert(f)
    local buf_sz = 1024 * 1024 * 4

    local buf = ""
    local eof = false

    local append = sha2.blake3() as sha2.Partial

    while not eof do
        buf = f:read(buf_sz)
        append = append(buf)
        if #buf < buf_sz then
            eof = true
        end
        print(#buf)
    end

    local hash_val = append() as string

    local hash_ref_blake3 = "bfb4b90586d7a67f6b433dbd6f1b4106fe3cd4b4440e7b68f5472cd1b3b97cbe"
    assert(hash_val == hash_ref_blake3)

    print("hash_val", hash_val)
end
--]]

--[[
function actions.embeddings(_: Args)
    local o: EmbeddingIndexOpts = {
        zlib_window = 15,
        zlib_level = 6,
        tags_fname = 'tags.json',
    }

    local chunks: {Chunk} = tags2chunks(o)
    if chunks_write_binary("chunks.bin", o, chunks) then
        print("chunks were written succesfully")
    end
end
--]]

--[[
function actions.chunks_open(_: Args)
    local chunks_str = load_chunks2string("chunks_koh.zlib")
    local chunks_sz = #chunks_str
    printc("%{blue}" .. format_size(chunks_sz) .. "%{reset}")

    local fn, errmsg = load(chunks_str)
    if not fn then
        print('errmsg', errmsg)
        return
    end

    local chunks = fn() as {Chunk}

    local dim = 1500
    local ef = 50
    local searcher = hnswlib.new(100000, dim, ef)

    local hash2chunk: {string: Chunk} = {}

    for _, ch in ipairs(chunks) do
        ch.hash = hash32(ch.text)
        assert(hash2chunk[ch.hash] == nil)
        hash2chunk[ch.hash] = ch
    end

    print("hashing was finished")

    local chunks_without_embeddings: {Chunk} = {}

    for _, ch in ipairs(chunks) do
        if ch.embedding then
            assert(type(ch.embedding) == 'table')
            searcher:add(ch.embedding as {number}, ch.hash)
        else
            print("no embedding vector for chunk", bin_to_hex(ch.hash))
            table.insert(chunks_without_embeddings, ch)
        end
    end

    for _, ch in ipairs(chunks_without_embeddings) do
        print(inspect(ch))
        print()
    end

    print("embedding were loaded")
    searcher:save("chunks.index")
end
--]]

--[[
function actions.load_index(_: Args)
    local chunks_str = load_chunks2string("chunks_koh.zlib")

    local fn, errmsg = load(chunks_str)
    if not fn then
        print('errmsg', errmsg)
        return
    end

    local chunks = fn() as {Chunk}
    local hash2chunk: {string: Chunk} = {}

    for _, ch in ipairs(chunks) do
        ch.hash = hash32(ch.text)
        assert(hash2chunk[ch.hash] == nil)
        hash2chunk[ch.hash] = ch
    end


    local searcher = hnswlib.load("chunks.index")

    local inp = readline(ansicolors("%{green}query: %{reset}"))

    local emb = assist.embedding(llm_embedding_model, inp)

    print()
    print()
    print()

    assert(emb[1])
    assert(#emb[1] == llm_embedding_dim)
    local nearest = searcher:search_str(emb[1], 10)
    for _, hash in ipairs(nearest) do
        print(bin_to_hex(hash))
    end

    for _, hash in ipairs(nearest) do
        local ch = hash2chunk[hash]
        if ch then
            print(ch.text)
        end
    end
end
--]]

--[[
function actions.files_koh(_args: Args)
    cmd_do("lms server start")

    ut.push_current_dir()
    chdir("src")
    cmd_do("ls -l")

    local extensions_t = {
        ".*\\.c$",
        ".*\\.cpp$",
        ".*\\.h$",
        ".*\\.hpp$",
        ".*\\.md$",
    }
    local files = _fd_code_in_cwd(extensions_t)

    print('files', inspect(files))
    ut.pop_dir()

    index_create({
        files = files,
        zlib_level = 3,
        zlib_window = 15,
        index_fname = "chunks_koh_tmp.zlib"
    })

end
--]]

--[[
function actions.updates(_args: Args)
    print("updates")
    for _, dep in ipairs(dependencies) do
        if dep.url and string.match(dep.url, "%.git$") then
            if dep.dir then
                print('dep.dir', dep.dir)
                ut.push_current_dir()
                chdir(path_rel_third_party .. "/" .. dep.dir)
                cmd_do({
                    "git fetch",
                    "git status",
                })
                ut.pop_dir()
            end
        end
    end
end
--]]

-- }}}

-- {{{ Закомментированный AI-чат

--[[
local mkd: {string} = {}

function actions.ai(_args: Args)
    print("actions.ai")

    if not lms.up() then
        print("actions.ai: lmstudio could not startup")
        return
    end

    local enum LoopState
        "main"
        "answer"
        "quit"
    end

    local running = true
    local loop_state: LoopState = 'main'
    local return_to_main = false

    local function handle_sigint(_: any)
        if loop_state == 'answer' then
            printc(
                "\n%{red}handle_sigint: return to main loop state%{reset}\n"
            )
            return_to_main = true
        else
            linenoise.save_history("history.txt")
            print("\n%{red}return to os%{reset}\n")
            os.exit(1)
        end
    end

    local signal = require("posix.signal")
    signal.signal(signal.SIGINT, handle_sigint)

    local models = assist.models_list()
    print(inspect(models))

    local welcome_str_escape = ansicolors("%{green}send message%{green}: ")
    local inp: string

    local base_message = { role = "system", content = system_prompt, }
    local chat: assist.LLMPayload = {
        model = llm_model,
        stream = true,
        messages = {
            base_message,
        },
    }

    local commands = {
        quit = function()
            printc("%{green}quit%{reset}")
            running = false
        end,
        exit = function()
            printc("%{green}exit%{reset}")
            running = false
        end,
        ctx_erase = function()
            chat.messages = {
                base_message,
            }
        end,
        ctx_view = function()
            for _, msg in ipairs(chat.messages) do
                local role = msg.role
                if role == "user" then
                    io.write(ansicolors("%{yellow}"))
                elseif role == "assistant" then
                    io.write(ansicolors("%{blue}"))
                end
                io.write(inspect(msg) .. "\n%{reset}")
            end
        end,
        help = function()
            local h = {
                'quit, exit - выйти на хрен',
                'help       - смотреть этот текст',
                'ctx_reset  - очистить историю, сбросить контекст',
                'ctx_view   - посмотреть содержимое истории',
                'query      - векторный поиск по коду',
            }
            for _, line in ipairs(h) do
                print(line)
            end
        end,
        query = function(carg: string)
            print("::query", carg)
        end,
        load = function()
        end
    }

    local function eval_commands(s: string): boolean
        local cmd, carg = string.match(s, "^::(%S+)(.*)")
        if not cmd then
            return
        end

        for k, func in pairs(commands as {string:function(carg?: string)})  do
            if cmd == k then
                func(carg)
                return true
            end
        end
        return false
    end

    linenoise.set_multiline(true)
    local markdown = markdown_instance()

    linenoise.load_history("history.txt")

    print("actions.ai: mainloop enter")

    while running do
        inp = linenoise.readline(welcome_str_escape)

        linenoise.add_history(inp)

        if eval_commands(inp) then
            goto continue
        end

        local texts = ""

        local f_tmp = io.open("contex.txt", "w")
        f_tmp:write(texts)
        f_tmp:close()

        if inp == "exit" or inp == "quit" then
            break
        end

        table.insert(chat.messages, {
            role = 'user',
            content = "Контекста кода : " .. texts ..
                      "\n Вопрос пользователя :" .. inp,
        })

        return_to_main = false
        local send2llm = assist.send2llm

        local function on_chunk(responce_chunk: string): boolean
            table.insert(mkd, responce_chunk)
            local formated = markdown.feed(responce_chunk)
            io.write(formated)
            loop_state = 'answer'
            if return_to_main then
                return true
            end
            return false
        end

        local responce = send2llm(chat, on_chunk)

        table.insert(chat.messages, {
            role = 'assistant',
            content = responce,
        })

        ::continue::
    end

    linenoise.save_history("history.txt")

    local w = io.open("mkd_chunks.lua", "w")
    w:write(serpent.dump(mkd))
end
--]]

-- }}}

-- {{{ Прочий закомментированный код

-- Aseprite data structures
--[[
local record Aseprite
    record Meta
        record Size
            w: integer
            h: integer
        end
        image: string
        size: Size

        app: string
        frameTags: string
        layers: string
        slices: string
        version: string
        scale: string
        format: string
    end
    record Frame
        record _Frame
            x: number
            y: number
            w: number
            h: number
        end
        frame: _Frame
        num: number
    end
    frames: {Frame}
    meta: Meta
end
--]]

--[[
function actions.anim_convert(_args: Args)
    print('anim_convert', inspect(_args))
    if not _args.name then
        print("There is no json file path in argument")
        os.exit(1)
    end

    local data = io.open(_args.name, "r"):read("*a")

    local js = json.decode(data) as Aseprite
    if not js then
        print("parsing error")
        os.exit(1)
    end

    local frames: {Aseprite.Frame} = {}
    for k, v in pairs(js.frames as {string:any}) do
        local frame = v as Aseprite.Frame
        frame.num = tonumber(string.match(k, "(%d*)%.aseprite"))
        table.insert(frames, frame)
    end

    table.sort(frames, function(a: Aseprite.Frame, b: Aseprite.Frame): boolean
        return a.num < b.num
    end)

    local res: Aseprite = {}
    res.meta = js.meta
    res.meta.app = nil
    res.meta.frameTags = nil
    res.meta.layers = nil
    res.meta.slices = nil
    res.meta.version = nil
    res.meta.scale = nil
    res.meta.format = nil
    res.frames = {}
    for _, frame in ipairs(frames) do
        table.insert(res.frames, {
            x = frame.frame.x,
            y = frame.frame.y,
            w = frame.frame.w,
            h = frame.frame.h,
        })
    end

    local new_fname = gsub(_args.name, "%.json$", ".lua")
    io.open(new_fname, "w"):write(serpent.dump(res))
end
--]]

-- yes_or_no
--[[
local enum Answer
    "yes"
    "no"
end

local function yes_or_no(input: string): Answer
    local yes = { "yes", "y", }
    local no = { "no", "n" }
    for _, v in ipairs(yes) do
        if input:lower() == v then
            return "yes"
        end
    end
    for _, v in ipairs(no) do
        if input:lower() == v then
            return "no"
        end
    end
    print("yes_or_no: bad value", input)
    os.exit(1)
end
--]]

-- update_links_table / update_links
--[[
local function update_links_table(_links: {string}, artifact: string)
    local found = false
    for _, line in ipairs(_links) do
        if string.match(line, artifact) then
            found = true
            break
        end
    end
    if not found then
        local ptrn = '<a href="https://nagolove.github.io/%s/"><strong>%s</strong></a>'
        table.insert(_links, format(ptrn, artifact, artifact))
    end
end

local function update_links(artifact: string)
    local site_repo_tmp = gsub(site_repo_index, "~", os.getenv("HOME"))
    local file = io.open(site_repo_tmp, "r")
    if not file then
        print(format("Could not load '%s' file", site_repo_tmp));
        os.exit(1)
    end

    local begin_section = "begin_links_section"
    local end_section = "end_links_section"

    local links_lines: {string} = {}
    local put = false
    local line_counter = 0
    local other_lines: {string} = {}
    for line in file:lines() do
        local begin = false
        if string.match(line, begin_section) then
            put = true
            begin = true
            goto continue
        end
        if string.match(line, end_section) then
            put = false
            goto continue
        end
        line_counter = line_counter + 1
        if put then
            table.insert(links_lines, line)
        end
        :: continue ::
        if (not put) or begin then
            table.insert(other_lines, line)
        end
    end

    if verbose then
        print('link_lines before update')
        print(tabular(links_lines))
    end

    update_links_table(links_lines, artifact)

    if verbose then
        print('link_lines after update')
        print(tabular(links_lines))
    end

    local new_lines: {string} = {}
    for _, line in ipairs(other_lines) do
        if string.match(line, begin_section) then
            table.insert(new_lines, line)
            for _, link_line in ipairs(links_lines) do
                table.insert(new_lines, link_line)
            end
            goto continue
        end
        table.insert(new_lines, line)
            :: continue ::
    end

    print('new_lines')
    print(tabular(new_lines))

    file = io.open(site_repo_tmp .. ".tmp", "w")
    for _, line in ipairs(new_lines) do
        file:write(line .. "\n")
    end
    file:close()

    local cmd1 = "mv " .. site_repo_tmp .. " " .. site_repo_tmp .. ".bak"
    local cmd2 = "mv " .. site_repo_tmp .. ".tmp " .. site_repo_tmp

    print(cmd1)
    print(cmd2)
end
--]]

-- dependencies_set_target
--[[
local function dependencies_set_target(target: Target)
    for _, dep in ipairs(modules) do
        dep.target = target
    end
end
--]]

-- strip_dotdot
--[[
local function strip_dotdot(path: string): string
    return (path:gsub("^%.%./", ""))
end
--]]

-- sanitizer flags
--[[
local flags_sanitazer = {
    "-fsanitize=undefined,address",
}
--]]

-- VerboseMode enum
--[[
local enum VerboseMode
    "all"
    "error"
end
--]]

-- Emscripten WASM flags
--[[
-- {{{ Специфические опции
#warning: Link with `-sLLD_REPORT_UNDEFINED` to get more information on undefined symbols
#warning: To disable errors for undefined symbols use `-sERROR_ON_UNDEFINED_SYMBOLS=0`

#emcc -sEVAL_CTORS

    local flags = {
        "-s USE_GLFW=3",
        "-s MAXIMUM_MEMORY=4294967296",
        "-s ALLOW_MEMORY_GROWTH=1",
        "-s EMULATE_FUNCTION_POINTER_CASTS",
        "-s LLD_REPORT_UNDEFINED",
        "--preload-file assets",
        "-Wall -flto -g3 -DPLATFORM_WEB",
        main_fname or '',
    }
-- }}}
--]]

-- w_header_v2
--[[
local function w_header_v2(f: FILE, o: EmbeddingIndexOpts, chunks: {Chunk})
    local embedding_dim = llm_embedding_dim
    -- формат заголовка:
    -- magic: u64
    -- version: u64
    -- chunks_num: u64
    -- embedding_dim: u64
    -- zlib_window: u64
    -- zlib_level: u64
    -- llm_embedding_model: string(64)

    local magic = 0x4B4F4831 -- "KOH1"
    local version = 2
    local h = sha2.blake3()

    w_u64(f, magic, h)
    w_u64(f, version, h)
    w_u64(f, #chunks, h)
    w_u64(f, embedding_dim, h)
    w_u64(f, o.zlib_window, h)
    w_u64(f, o.zlib_level, h)

    local llm = llm_embedding_model .. "\0"
    assert(llm and #llm <= 64, "invalid llm_embedding_model")
    w_str_fixed(f, llm, 64, h)

    local hash_val = h() as string
    assert(#hash_val == 64)
    f:write(hash_val)
end
--]]

-- LMS model loading block
--[[
-- XXX: С чем лучше сравнивать llm_model в is_loaded()?
if not lms.is_loaded(llm_model) then
    lms.load(llm_model)
    os.execute("sleep 2")
    local i = 0
    while not lms.is_loaded(llm_model) do
        os.execute("sleep 1")
        i = i + 1
        if i > 20 then
            printc(
                "Could not load %{blue}" ..
                llm_model ..
                "%{reset} model"
            )
            return
        end
    end
end
--]]

-- }}}
