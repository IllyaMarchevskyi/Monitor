#!/usr/bin/env python3
import glob
import os
import shutil
import subprocess
import sys


def find_hex_files():
    return sorted(
        f for f in glob.glob("*.hex") if os.path.isfile(f)
    )


def choose_from_list(items, prompt):
    if not items:
        return None
    # if len(items) == 1:
    #     print(f"{prompt} {items[0]}")
    #     return items[0]
    for i, item in enumerate(items, 1):
        print(f"{i}) {item}")
    while True:
        choice = input(f"{prompt} [1-{len(items)}]: ").strip()
        if not choice:
            continue
        if choice.isdigit():
            idx = int(choice)
            if 1 <= idx <= len(items):
                return items[idx - 1]
        print("Invalid выбор, спробуй ще раз.")


def list_ports_pyserial():
    try:
        from serial.tools import list_ports
    except Exception:
        return None
    ports = []
    for p in list_ports.comports():
        label = f"{p.device} - {p.description}" if p.description else p.device
        ports.append(label)
    return ports


def list_ports_fallback():
    if os.name == "nt":
        ps_cmd = [
            "powershell",
            "-NoProfile",
            "-Command",
            "(Get-ItemProperty -Path 'HKLM:\\HARDWARE\\DEVICEMAP\\SERIALCOMM' | "
            "Select-Object -Property * -ExcludeProperty PSPath,PSParentPath,PSChildName,PSDrive,PSProvider)."
            "PSObject.Properties.Value",
        ]
        try:
            out = subprocess.check_output(ps_cmd, text=True, stderr=subprocess.DEVNULL)
            ports = [line.strip() for line in out.splitlines() if line.strip()]
            if ports:
                return ports
        except Exception:
            pass
        ports = [f"COM{i}" for i in range(1, 257)]
        return ports
    patterns = [
        "/dev/ttyUSB*",
        "/dev/ttyACM*",
        "/dev/tty.usb*",
        "/dev/tty.wchusbserial*",
        "/dev/tty.serial*",
    ]
    ports = []
    for pat in patterns:
        ports.extend(glob.glob(pat))
    return sorted(set(ports))


def list_ports():
    ports = list_ports_pyserial()
    if ports:
        return ports
    return list_ports_fallback()


def find_avrdude():
    path = shutil.which("avrdude")
    if path:
        return path, find_avrdude_conf(path)

    candidates = []
    home = os.path.expanduser("~")

    if os.name == "nt":
        candidates += glob.glob(
            os.path.join(home, "AppData", "Local", "Arduino15", "packages",
                         "arduino", "tools", "avrdude", "*", "bin", "avrdude.exe")
        )
        candidates += glob.glob(
            os.path.join(home, ".platformio", "packages", "tool-avrdude", "avrdude.exe")
        )
        candidates += [
            r"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avrdude.exe",
            r"C:\Program Files\Arduino\hardware\tools\avr\bin\avrdude.exe",
        ]
    elif sys.platform == "darwin":
        candidates += glob.glob(
            os.path.join(home, "Library", "Arduino15", "packages", "arduino",
                         "tools", "avrdude", "*", "bin", "avrdude")
        )
        candidates += glob.glob(
            os.path.join(home, ".platformio", "packages", "tool-avrdude", "avrdude")
        )
        candidates += [
            "/Applications/Arduino.app/Contents/Java/hardware/tools/avr/bin/avrdude",
        ]
    else:
        candidates += glob.glob(
            os.path.join(home, ".arduino15", "packages", "arduino", "tools",
                         "avrdude", "*", "bin", "avrdude")
        )
        candidates += glob.glob(
            os.path.join(home, ".platformio", "packages", "tool-avrdude", "avrdude")
        )
        candidates += ["/usr/bin/avrdude", "/usr/local/bin/avrdude"]

    for c in candidates:
        if os.path.isfile(c):
            return c, find_avrdude_conf(c)
    return None, None


def find_avrdude_conf(avrdude_path):
    base = os.path.dirname(avrdude_path)
    candidates = [
        os.path.join(base, "avrdude.conf"),
        os.path.join(base, "..", "etc", "avrdude.conf"),
        os.path.join(base, "..", "avrdude.conf"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return os.path.abspath(c)
    return None


def extract_port_label(port_item):
    if " - " in port_item:
        return port_item.split(" - ", 1)[0].strip()
    return port_item.strip()


def choose_port_interactive():
    last_ports = None
    while True:
        ports = list_ports()
        if not ports:
            print("Портів не знайдено. Підключи пристрій і натисни Enter для оновлення.")
            input("")
            continue
        if ports != last_ports:
            print("Доступні порти:")
            for i, item in enumerate(ports, 1):
                print(f"{i}) {item}")
            last_ports = ports
        choice = input("Вибери порт [номер], Enter=оновити, q=вихід: ").strip().lower()
        if choice == "q":
            return None
        if choice == "" or choice == "r":
            continue
        if choice.isdigit():
            idx = int(choice)
            if 1 <= idx <= len(ports):
                return extract_port_label(ports[idx - 1])
        print("Некоректний вибір.")


def main():
    hex_files = find_hex_files()
    if not hex_files:
        print("Не знайдено .hex у поточній папці.")
        sys.exit(1)
    hex_file = choose_from_list(hex_files, "Вибери .hex файл:")

    port = choose_port_interactive()
    if not port:
        print("Скасовано користувачем.")
        sys.exit(1)

    avrdude, avrdude_conf = find_avrdude()
    if not avrdude:
        print("avrdude не знайдено. Встанови Arduino IDE або PlatformIO, або додай avrdude в PATH.")
        sys.exit(1)

    cmd = [
        avrdude,
        "-p", "m2560",
        "-c", "wiring",
        "-P", port,
        "-b", "115200",
        "-D",
        "-U", f"flash:w:{hex_file}:i",
    ]
    if avrdude_conf:
        cmd.insert(1, "-C")
        cmd.insert(2, avrdude_conf)

    print("Запуск прошивки...")
    print(" ".join(cmd))
    res = subprocess.call(cmd)
    if res != 0:
        print(f"Прошивка не вдалася. Код помилки: {res}")
        sys.exit(res)
    print("Готово.")


if __name__ == "__main__":
    main()
