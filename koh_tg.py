#!/usr/bin/env python3
# Отправка файлов в общий Telegram-канал сборок (для `koh tg`).
# Одна авторизация telethon на все проекты: session и pid-lock в ~/.config/koh/.
# Настройки — env TG_* или ~/.config/koh/tg.env (env имеет приоритет).
import argparse
import asyncio
import os
import sys
from urllib.parse import urlparse

CFG_DIR = os.path.expanduser("~/.config/koh")
ENV_FILE = os.path.join(CFG_DIR, "tg.env")
# KOH_TG_SESSION — другой путь session (без .session), для проверок входа
SESSION = os.environ.get("KOH_TG_SESSION") or os.path.join(CFG_DIR, "koh_tg")
LOCK_FILE = SESSION + ".pid"

ENV_TEMPLATE = """\
# ~/.config/koh/tg.env — настройки `koh tg`, формат KEY=value
# api_id/api_hash: https://my.telegram.org -> API development tools
TG_API_ID=123456
TG_API_HASH=0123456789abcdef0123456789abcdef
# id канала (-100...) или @username
TG_CHANNEL=-1001234567890
# телефон аккаунта, нужен только при первой авторизации
TG_PHONE=+7XXXXXXXXXX
# необязательно: socks5://host:port | socks4://... | http://...
#TG_PROXY=socks5://127.0.0.1:1080
"""


def load_env_file():
    # Значения из файла не перекрывают уже заданные переменные окружения
    if not os.path.isfile(ENV_FILE):
        return
    with open(ENV_FILE) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, val = line.split("=", 1)
            os.environ.setdefault(key.strip(), val.strip().strip('"\''))


# Код выхода при Ctrl+C (128 + SIGINT), как у shell
EXIT_SIGINT = 130


def die(msg, code=1):
    print(f"koh_tg: {msg}", file=sys.stderr)
    sys.exit(code)


def parse_proxy():
    url = os.environ.get("TG_PROXY", "")
    if not url:
        return None
    import socks
    p = urlparse(url)
    scheme_map = {"socks5": socks.SOCKS5, "socks4": socks.SOCKS4,
                  "http": socks.HTTP}
    st = scheme_map.get(p.scheme.lower())
    if st is None:
        die(f"неизвестная схема прокси: {p.scheme}")
    return (st, p.hostname or "127.0.0.1", p.port or 1080)


def channel_ref(s):
    # Числовой id канала или @username
    try:
        return int(s)
    except ValueError:
        return s


class PidLock:
    # Не даёт двум отправкам одновременно писать в одну session-базу
    def acquire(self):
        if os.path.exists(LOCK_FILE):
            try:
                with open(LOCK_FILE) as f:
                    pid = int(f.read().strip())
                os.kill(pid, 0)
                die(f"уже запущен экземпляр (PID {pid}), lock: {LOCK_FILE}")
            except (ValueError, ProcessLookupError):
                os.remove(LOCK_FILE)
        with open(LOCK_FILE, "w") as f:
            f.write(str(os.getpid()))

    def release(self):
        try:
            with open(LOCK_FILE) as f:
                if int(f.read().strip()) == os.getpid():
                    os.remove(LOCK_FILE)
        except (ValueError, OSError):
            pass


def progress(current, total):
    mb = 1024 * 1024
    print(f"\rЗагрузка: {current * 100 / total:.1f}% "
          f"({current / mb:.1f}/{total / mb:.1f} MB)", end="", flush=True)


# Куда Telegram отправил код входа — по имени класса SentCodeType*
CODE_WHERE = {
    "SentCodeTypeApp": "в служебный чат «Telegram» (синяя галочка, 777000) "
                       "в приложении этого аккаунта — не бот Verification Codes",
    "SentCodeTypeSms": "SMS на номер",
    "SentCodeTypeSmsWord": "SMS со словом на номер",
    "SentCodeTypeSmsPhrase": "SMS с фразой на номер",
    "SentCodeTypeFirebaseSms": "SMS на номер",
    "SentCodeTypeCall": "голосовой звонок с кодом",
    "SentCodeTypeFlashCall": "сброшенный звонок: код — цифры номера",
    "SentCodeTypeMissedCall": "пропущенный звонок: код — последние цифры номера",
    "SentCodeTypeEmailCode": "письмо на email аккаунта",
    "SentCodeTypeSetUpEmailRequired": "Telegram требует привязать email "
                                      "(сделайте в приложении)",
    "SentCodeTypeFragmentSms": "SMS через fragment.com (анонимный номер)",
}


def mask_phone(phone):
    return phone[:4] + "…" + phone[-2:] if len(phone) > 6 else phone


def print_code_type(sent, phone):
    t = type(sent.type).__name__
    print(f"Код для {mask_phone(phone)}: {CODE_WHERE.get(t, t)}")
    nxt = getattr(sent, "next_type", None)
    if nxt is not None:
        n = type(nxt).__name__
        print(f"  пустой Enter — переотправить: {CODE_WHERE.get(n, n)}")
    timeout = getattr(sent, "timeout", None)
    if timeout:
        print(f"  переотправка доступна через {timeout} сек")


def new_client():
    from telethon import TelegramClient
    api_id = int(os.environ.get("TG_API_ID", "0") or 0)
    api_hash = os.environ.get("TG_API_HASH", "")
    return TelegramClient(SESSION, api_id, api_hash, proxy=parse_proxy())


async def connect():
    # Только готовая session: отправка из сборки не должна ждать ввода кода
    client = new_client()
    await client.connect()
    if not await client.is_user_authorized():
        await client.disconnect()
        die(f"нет авторизации ({SESSION}.session), выполните: koh tg --login")
    return client


async def sign_in(client):
    # Явный вход: показать способ доставки кода, Enter — переотправка,
    # при 2FA — пароль облачного входа
    from getpass import getpass
    from telethon import functions
    from telethon.errors import (FloodWaitError, PhoneCodeExpiredError,
                                 PhoneCodeInvalidError, PhoneNumberInvalidError,
                                 SessionPasswordNeededError)

    phone = os.environ.get("TG_PHONE", "") or input("Телефон (+7…): ").strip()
    try:
        sent = await client.send_code_request(phone)
    except PhoneNumberInvalidError:
        die("неверный номер телефона, формат: +7XXXXXXXXXX")
    except FloodWaitError as e:
        die(f"слишком много попыток, подождите {e.seconds} сек")
    print_code_type(sent, phone)

    while True:
        code = input("Код (Enter — переотправить): ").strip()
        if not code:
            try:
                sent = await client(functions.auth.ResendCodeRequest(
                    phone, sent.phone_code_hash))
            except FloodWaitError as e:
                print(f"переотправка недоступна ещё {e.seconds} сек")
                continue
            except Exception as e:
                print(f"переотправка не удалась: {e}")
                continue
            print_code_type(sent, phone)
            continue
        try:
            await client.sign_in(phone, code,
                                 phone_code_hash=sent.phone_code_hash)
            return
        except PhoneCodeInvalidError:
            print("неверный код, ещё раз")
        except PhoneCodeExpiredError:
            die("код истёк, запустите koh tg --login заново")
        except SessionPasswordNeededError:
            await client.sign_in(password=getpass("Пароль 2FA: "))
            return


async def login():
    # Разовая авторизация и проверка доступа к каналу
    client = new_client()
    await client.connect()
    try:
        if not await client.is_user_authorized():
            await sign_in(client)
        me = await client.get_me()
        print(f"Авторизовано: {me.first_name} (@{me.username}), "
              f"session: {SESSION}.session")
        await client.get_dialogs()
        channel = os.environ.get("TG_CHANNEL", "")
        entity = await client.get_entity(channel_ref(channel))
        print(f"Канал доступен: {getattr(entity, 'title', channel)}")
    finally:
        await client.disconnect()


async def send(files, caption):
    channel = os.environ.get("TG_CHANNEL", "")
    client = await connect()
    try:
        await client.get_dialogs()
        entity = await client.get_entity(channel_ref(channel))
        print(f"Отправка {len(files)} файл(ов) в {channel}...")
        await client.send_file(
            entity, files if len(files) > 1 else files[0],
            caption=caption, grouped=len(files) > 1,
            progress_callback=progress,
        )
        print("\nОтправлено успешно.")
    finally:
        await client.disconnect()


def main():
    p = argparse.ArgumentParser(
        description="Отправка файлов в общий Telegram-канал сборок. "
                    f"Настройки: env TG_* или {ENV_FILE}.")
    p.add_argument("files", nargs="*", help="файлы для отправки")
    p.add_argument("-c", "--caption", help="подпись (по умолчанию имя файла)")
    p.add_argument("--template", action="store_true",
                   help="напечатать шаблон tg.env и выйти")
    p.add_argument("--login", action="store_true",
                   help="авторизоваться (телефон + код) и проверить канал")
    p.add_argument("--from", dest="from_session", metavar="SESSION",
                   help="с --login: взять готовую telethon-session вместо входа")
    args = p.parse_args()

    if args.template:
        print(ENV_TEMPLATE, end="")
        return
    if not args.files and not args.login:
        p.error("укажите файлы или --login")

    os.makedirs(CFG_DIR, exist_ok=True)
    load_env_file()
    need = ("TG_API_ID", "TG_API_HASH", "TG_CHANNEL")
    missing = [k for k in need if not os.environ.get(k)]
    if missing:
        print(f"koh_tg: не заданы {', '.join(missing)}.\n"
              f"Создайте {ENV_FILE} по шаблону:\n", file=sys.stderr)
        print(ENV_TEMPLATE, file=sys.stderr)
        sys.exit(1)

    if args.from_session and not args.login:
        p.error("--from используется только с --login")

    if args.login:
        if args.from_session:
            if not os.path.isfile(args.from_session):
                die(f"session не найдена: {args.from_session}")
            import shutil
            shutil.copyfile(args.from_session, SESSION + ".session")
            print(f"session скопирована в {SESSION}.session")
        lock = PidLock()
        lock.acquire()
        try:
            asyncio.run(login())
        finally:
            lock.release()
        return

    for fp in args.files:
        if not os.path.isfile(fp):
            die(f"файл не найден: {fp}")

    caption = args.caption or os.path.basename(args.files[0])
    lock = PidLock()
    lock.acquire()
    try:
        asyncio.run(send(args.files, caption))
    except KeyboardInterrupt:
        die("прервано, проверьте канал — файл мог уйти частично", EXIT_SIGINT)
    finally:
        lock.release()


if __name__ == "__main__":
    # Ctrl+C / Ctrl+D в любом месте (ввод кода, подключение) — без трассы;
    # lock снимают finally внутри main
    try:
        main()
    except (KeyboardInterrupt, EOFError):
        print(file=sys.stderr)
        die("прервано", EXIT_SIGINT)
