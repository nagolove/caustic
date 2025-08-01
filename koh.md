Задача - сделать бинарник системы сборки. 
Все исходники, в том числе библиотеки третьих лиц должны лежать рядом.

koh.c -> koh.exe

koh_src

Делаю ai ассистента для своей сборочной системы. Есть фреймворк caustic, его схема.
`reset && tree . -L 2`
Код сборочной системы и ассистена на луа.

Ты луа программист, специалист по llm, семантическому поиску и созданию 
ai-агентов.

Я делаю llm ассистента для своей сборочной системы и фреймворка. 
Фреймворк использует библиотеки третьих лиц и мой код. Он предназначен для
разработки игр. Проекты - игровые приложения, их каталоги лежат вне фреймворка.

Система сборки написана на луа. Код фреймворка - C и С++. 
Сперва я получаю весь код и делаю чанки в виде луа таблиц. 
Затем делаю кеш при помощи hnswlib. 
Затем создаю промпт и отправляю запрос к lm-studio через http апи.

Все работает как cli приложение - сборка, создание проекта и ассистент в одной
программе.

К либе hnswlib надо сделать луа интерфейс.

Спроси непонятные детали.
Вопрос - как я могу организовать выполнение действий, обратную связь от llm?
LLM дает ответ - типа дай инфу по таким функциям. Как это сделать, что-бы 
работало не только в одну сторону.

Получить ast для данного файла
clang++ -Xclang -ast-dump -fsyntax-only src/h_main.c



--------------------------



1. Хранение метаинформации
Ты векторизуешь чанки кода → у тебя есть:

Вектор (float[])

Идентификатор чанка (файл + offset, hash и т.д.)

Возможно текст чанка

Redis может быстро хранить и возвращать:

lua
Копировать
Редактировать
redis:set("chunk:<id>:text", chunk_text)
redis:set("chunk:<id>:file", file_path)
redis:set("chunk:<id>:line_start", line)
Это работает в паре с HNSWLib, где label → id, а Redis возвращает «человеческую» инфу.


local chunk_lines_num = 40
local chunk_lines_overlap = 5

local chunk = {
  id = file_path .. ":" .. line_start
  file = "src/koh_animator.c",
  line_start = 120, 
  line_end = 160,
  text = "...код чанка...",
  hash = "ab12cd...", 
  ts_updated = os.time()
}


Как работает схема с hnswlib
1. ✂️ Разбивка кода на чанки (ты уже делаешь)

local chunk = {
  id = "chunk_0042",
  file = "src/koh_animator.c",
  line_start = 120,
  line_end = 150,
  text = "...код чанка...",
}

2. 🔢 Векторизация через OpenAI /v1/embeddings
Ты отправляешь chunk.text на векторизацию:

lua
Копировать
Редактировать
local embedding = get_embedding(chunk.text) -- возвращает float[1536]
chunk.embedding = embedding
3. 🧠 Индексация в hnswlib
Ты добавляешь вектор в индекс:

hnsw:add_item(embedding, chunk.id)
chunk.id может быть:

chunk_0042 (строка)

или 42 (int) — зависит от того, как ты инициализировал hnswlib

4. 🔍 Поиск по вопросу
Когда пользователь вводит:

arduino
Копировать
Редактировать
"Как работает управление слоями в koh_animator?"
Ты:

Векторизуешь запрос.

Ищешь ближайшие чанки:

lua
Копировать
Редактировать
local query_embedding = get_embedding(user_query)
local results = hnsw:search(query_embedding, top_k)
results → массив { id = chunk_id, distance = score }

Ты по этим id вытягиваешь данные из Redis:

lua
Копировать
Редактировать
local chunk = meta.load_chunk(id)
Собираешь результаты → показываешь/отправляешь в LLM.

📚 Роль hnswlib — кратко:
Шаг	Задача	Инструмент
1.	Преобразование текста в вектор	OpenAI /v1/embeddings
2.	Быстрый поиск по векторам	hnswlib
3.	Сопоставление ID → текст	Redis (или Lua-таблицы)

💡 Какой hash использовать?
Для поля chunk.hash, если тебе нужен:

Цель	Рекомендация
Быстрое сравнение изменений	md5 (хватает)
Проверка целостности	sha1 или sha256
Встроенный в Lua	Используй md5 от Lua или OpenResty ngx.md5

Пример на Lua:

lua
Копировать
Редактировать
local md5 = require("md5")
chunk.hash = md5.sumhexa(chunk.text)
⚠️ Примечание по chunk.id
Если ты делаешь глобально уникальный id, лучше не полагаться на int-последовательность, а использовать:

lua
Копировать
Редактировать
chunk.id = file_hash .. ":" .. line_start
-- или
chunk.id = base64(sha1(chunk.text)):sub(1, 12)
Так проще мапить обратно в HNSW и избегать коллизий между файлами.


❓ Сколько строк брать, если нет анализа AST или ctags?
Ответ:
Если нет AST или токенизации — бери фиксированный размер, например:
32, 48, 64 строк на чанк


❓ Каким алгоритмом считать хеш?

Ответ:

Если нужно просто отслеживать, изменился ли чанк — md5 достаточно

Если хочется стойкости — sha1 или sha256

В Lua это можно реализовать через:

md5.lua (pure Lua)

OpenResty (ngx.md5)

Обвязку через внешний openssl (если нужен CLI)



Поле	Значение/назначение
id	Лучше file:line_start, можно hash
file	Относительный путь
line_start	Начальная строка чанка
line_end	Строка окончания (обычно start + chunk_size - 1)
text	Исходный текст чанка
hash	md5(text) для отслеживания изменений
ts_updated	os.time() при векторизации/перестроении

⚖️ Рекомендованное значение:
chunk_size = 48

overlap = 8 (или 10–20%)

Да, xxhash отлично подойдёт в твоём случае

luarocks install cffi-lua --local



| Модуль                                  | Греческая буква | Транслитерация | Значение/обоснование                             |
| --------------------------------------- | --------------- | -------------- | ------------------------------------------------ |
| AI ассистент                            | **μ**           | `mu`           | микроскопический интеллект, импульсы             |
| Векторный рендер (SVG)                  | **λ**           | `lambda`       | трансформации, чистые функции, графические формы |
| Векторные контуры (Box2D и др.)         | **ξ**           | `xi`           | геометрия, координаты, сложные формы             |
| Отслеживание изменений (внешние модули) | **ϵ**           | `epsilon`      | малые изменения, пределы, контроль               |
| Система тестирования                    | **τ**           | `tau`          | тесты времени, устойчивость                      |
| Выкладка, Git push/CI                   | **π**           | `pi`           | циклы, непрерывность, итерации                   |

| Греческий символ | Транскрипция (латиница) | Использование                          |
| ---------------- | ----------------------- | -------------------------------------- |
| **Ψ**            | `psi`                   | `refactoring`                          |
| **ζ**            | `zeta`                  | `sprite editor tool`                   |
| **𝝇** (rho)     | `rho`                   | `performance` (feature vs. lag as bug) |
| **α**            | `alpha`                 | `it is not a stream`                   |
| **τ**            | `tau`                   | `testsuite`                            |

mu - ai ассистент
xi - векторная система
tau - тестирование, устойчивость

Ты помогаешь с кодом и отвечаешь только, если понял контекст. Если в вопросе не
хватает технических терминов, верни [NEED_REFINE] и уточни, что именно нужно
указать.

local response = call_llm(prompt)
if response:match("%[NEED_REFINE%]") then
  print("⚠️  Ассистенту не хватает информации:")
  print(response:gsub("%[NEED_REFINE%]%s*", "")) -- чистим маркер
  return  -- не делаем запрос к кодовому контексту
end


