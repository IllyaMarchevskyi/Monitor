Import("env")

import os
from pathlib import Path


PROJECT_DIR = Path(env["PROJECT_DIR"])


def load_dotenv(path):
    values = {}
    if not path.is_file():
        return values

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip("\"'")
        if key:
            values[key] = value

    return values


DOTENV_VALUES = load_dotenv(PROJECT_DIR / ".env")


def define_if_present(name):
    value = os.environ.get(name, DOTENV_VALUES.get(name))
    if value:
        env.Append(CPPDEFINES=[(f"{name}_VALUE", '\\"%s\\"' % value)])


define_if_present("SERVER_IP")
define_if_present("API_KEY")
define_if_present("VERSION")
