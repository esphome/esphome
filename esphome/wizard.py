import os
import random
import string
import unicodedata

import voluptuous as vol

import esphome.config_validation as cv
from esphome.const import ALLOWED_NAME_CHARS, ENV_QUICKWIZARD
from esphome.core import CORE
from esphome.helpers import get_bool_env, write_file
from esphome.log import Fore, color
from esphome.storage_json import StorageJSON, ext_storage_path
from esphome.util import safe_input, safe_print

CORE_BIG = r"""    _____ ____  _____  ______
   / ____/ __ \|  __ \|  ____|
  | |   | |  | | |__) | |__
  | |   | |  | |  _  /|  __|
  | |___| |__| | | \ \| |____
   \_____\____/|_|  \_\______|
"""
ESP_BIG = r"""      ______  _____ _____
     |  ____|/ ____|  __ \\
     | |__  | (___ | |__) |
     |  __|  \___ \|  ___/
     | |____ ____) | |
     |______|_____/|_|
"""
WIFI_BIG = r"""   __          ___ ______ _
   \ \        / (_)  ____(_)
    \ \  /\  / / _| |__   _
     \ \/  \/ / | |  __| | |
      \  /\  /  | | |    | |
       \/  \/   |_|_|    |_|
"""
OTA_BIG = r"""       ____ _______
      / __ \__   __|/\\
     | |  | | | |  /  \\
     | |  | | | | / /\ \\
     | |__| | | |/ ____ \\
      \____/  |_/_/    \_\\
"""

BASE_CONFIG = """esphome:
  name: {name}
"""

BASE_CONFIG_FRIENDLY = """esphome:
  name: {name}
  friendly_name: {friendly_name}
"""

LOGGER_CONFIG = """
# Enable logging
logger:
"""

API_CONFIG = """
# Enable Home Assistant API
api:
"""

ESP8266_CONFIG = """
esp8266:
  board: {board}
"""

ESP32_CONFIG = """
esp32:
  board: {board}
  framework:
    type: arduino
"""

ESP32S2_CONFIG = """
esp32:
  board: {board}
  framework:
    type: esp-idf
"""

ESP32C3_CONFIG = """
esp32:
  board: {board}
  framework:
    type: esp-idf
"""

RP2040_CONFIG = """
rp2040:
  board: {board}
  framework:
    # Required until https://github.com/platformio/platform-raspberrypi/pull/36 is merged
    platform_version: https://github.com/maxgerhardt/platform-raspberrypi.git
"""

BK72XX_CONFIG = """
bk72xx:
  board: {board}
"""

RTL87XX_CONFIG = """
rtl87xx:
  board: {board}
"""

HARDWARE_BASE_CONFIGS = {
    "ESP8266": ESP8266_CONFIG,
    "ESP32": ESP32_CONFIG,
    "ESP32S2": ESP32S2_CONFIG,
    "ESP32C3": ESP32C3_CONFIG,
    "RP2040": RP2040_CONFIG,
    "BK72XX": BK72XX_CONFIG,
    "RTL87XX": RTL87XX_CONFIG,
}


def sanitize_double_quotes(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def wizard_file(**kwargs):
    letters = string.ascii_letters + string.digits
    ap_name_base = kwargs["name"].replace("_", " ").title()
    ap_name = f"{ap_name_base} Fallback Hotspot"
    if len(ap_name) > 32:
        ap_name = ap_name_base
    kwargs["fallback_name"] = ap_name
    kwargs["fallback_psk"] = "".join(random.choice(letters) for _ in range(12))

    if kwargs.get("friendly_name"):
        base = BASE_CONFIG_FRIENDLY
    else:
        base = BASE_CONFIG

    config = base.format(**kwargs)

    config += HARDWARE_BASE_CONFIGS[kwargs["platform"]].format(**kwargs)

    config += LOGGER_CONFIG

    if kwargs["board"] == "rpipico":
        return config

    config += API_CONFIG

    # Configure API
    if "password" in kwargs:
        config += f"  password: \"{kwargs['password']}\"\n"
    if "api_encryption_key" in kwargs:
        config += f"  encryption:\n    key: \"{kwargs['api_encryption_key']}\"\n"

    # Configure OTA
    config += "\nota:\n"
    config += "  - platform: esphome\n"
    if "ota_password" in kwargs:
        config += f"    password: \"{kwargs['ota_password']}\""
    elif "password" in kwargs:
        config += f"    password: \"{kwargs['password']}\""

    # Configuring wifi
    config += "\n\nwifi:\n"

    if "ssid" in kwargs:
        if kwargs["ssid"].startswith("!secret"):
            template = "  ssid: {ssid}\n  password: {psk}\n"
        else:
            template = """  ssid: "{ssid}"\n  password: "{psk}"\n"""
        config += template.format(**kwargs)
    else:
        config += """  # ssid: "My SSID"
  # password: "mypassword"

  networks:
"""

    # pylint: disable=consider-using-f-string
    if kwargs["platform"] in ["ESP8266", "ESP32", "BK72XX", "RTL87XX"]:
        config += """
  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "{fallback_name}"
    password: "{fallback_psk}"

captive_portal:
    """.format(
            **kwargs
        )
    else:
        config += """
  # Enable fallback hotspot in case wifi connection fails
  ap:
    ssid: "{fallback_name}"
    password: "{fallback_psk}"
    """.format(
            **kwargs
        )

    return config


def wizard_write(path, **kwargs):
    from esphome.components.bk72xx import boards as bk72xx_boards
    from esphome.components.esp32 import boards as esp32_boards
    from esphome.components.esp8266 import boards as esp8266_boards
    from esphome.components.rp2040 import boards as rp2040_boards
    from esphome.components.rtl87xx import boards as rtl87xx_boards

    name = kwargs["name"]
    board = kwargs["board"]

    for key in ("ssid", "psk", "password", "ota_password"):
        if key in kwargs:
            kwargs[key] = sanitize_double_quotes(kwargs[key])

    if "platform" not in kwargs:
        if board in esp8266_boards.BOARDS:
            platform = "ESP8266"
        elif board in esp32_boards.BOARDS:
            platform = "ESP32"
        elif board in rp2040_boards.BOARDS:
            platform = "RP2040"
        elif board in bk72xx_boards.BOARDS:
            platform = "BK72XX"
        elif board in rtl87xx_boards.BOARDS:
            platform = "RTL87XX"
        else:
            safe_print(color(Fore.RED, f'The board "{board}" is unknown.'))
            return False
        kwargs["platform"] = platform
    hardware = kwargs["platform"]

    write_file(path, wizard_file(**kwargs))
    storage = StorageJSON.from_wizard(name, name, f"{name}.local", hardware)
    storage_path = ext_storage_path(os.path.basename(path))
    storage.save(storage_path)

    return True


if get_bool_env(ENV_QUICKWIZARD):

    def sleep(time):
        pass

else:
    from time import sleep


def safe_print_step(step, big):
    safe_print()
    safe_print()
    safe_print(f"============= STEP {step} =============")
    safe_print(big)
    safe_print("===================================")
    sleep(0.25)


def default_input(text, default):
    safe_print()
    safe_print(f"Press ENTER for default ({default})")
    return safe_input(text.format(default)) or default


# From https://stackoverflow.com/a/518232/8924614
def strip_accents(value):
    return "".join(
        c
        for c in unicodedata.normalize("NFD", str(value))
        if unicodedata.category(c) != "Mn"
    )


def wizard(path):
    from esphome.components.bk72xx import boards as bk72xx_boards
    from esphome.components.esp32 import boards as esp32_boards
    from esphome.components.esp8266 import boards as esp8266_boards
    from esphome.components.rtl87xx import boards as rtl87xx_boards
    from esphome.components.rp2040 import boards as rp2040_boards

    if not path.endswith(".yaml") and not path.endswith(".yml"):
        safe_print(
            f"Please make your configuration file {color(Fore.CYAN, path)} have the extension .yaml or .yml"
        )
        return 1
    if os.path.exists(path):
        safe_print(
            f"Uh oh, it seems like {color(Fore.CYAN, path)} already exists, please delete that file first or chose another configuration file."
        )
        return 2

    CORE.config_path = path

    safe_print("Hi there!")
    sleep(1.5)
    safe_print("I'm the wizard of ESPHome :)")
    sleep(1.25)
    safe_print("And I'm here to help you get started with ESPHome.")
    sleep(2.0)
    safe_print(
        "In 4 steps I'm going to guide you through creating a basic "
        "configuration file for your custom firmware. Yay!"
    )
    sleep(3.0)
    safe_print()
    safe_print_step(1, CORE_BIG)
    safe_print(f"First up, please choose a {color(Fore.GREEN, 'name')} for your node.")
    safe_print(
        "It should be a unique name that can be used to identify the device later."
    )
    sleep(1)
    safe_print(
        f"For example, I like calling the node in my living room {color(Fore.BOLD_WHITE, 'livingroom')}."
    )
    safe_print()
    sleep(1)
    name = safe_input(color(Fore.BOLD_WHITE, "(name): "))

    while True:
        try:
            name = cv.valid_name(name)
            break
        except vol.Invalid:
            safe_print(
                color(
                    Fore.RED,
                    f'Oh noes, "{name}" isn\'t a valid name. Names can only '
                    f"include numbers, lower-case letters and hyphens. ",
                )
            )
            name = strip_accents(name).lower().replace(" ", "-")
            name = strip_accents(name).lower().replace("_", "-")
            name = "".join(c for c in name if c in ALLOWED_NAME_CHARS)
            safe_print(f'Shall I use "{color(Fore.CYAN, name)}" as the name instead?')
            sleep(0.5)
            name = default_input("(name [{}]): ", name)

    safe_print(f'Great! Your node is now called "{color(Fore.CYAN, name)}".')
    sleep(1)
    safe_print_step(2, ESP_BIG)
    safe_print(
        "Now I'd like to know what microcontroller you're using so that I can compile "
        "firmwares for it."
    )

    wizard_platforms = ["ESP32", "ESP8266", "BK72XX", "RTL87XX", "RP2040"]
    safe_print(
        "Please choose one of the supported microcontrollers "
        "(Use ESP8266 for Sonoff devices)."
    )
    while True:
        sleep(0.5)
        safe_print()
        platform = safe_input(
            color(Fore.BOLD_WHITE, f"({'/'.join(wizard_platforms)}): ")
        )
        try:
            platform = vol.All(vol.Upper, vol.Any(*wizard_platforms))(platform.upper())
            break
        except vol.Invalid:
            safe_print(
                f'Unfortunately, I can\'t find an espressif microcontroller called "{platform}". Please try again.'
            )
    safe_print(f"Thanks! You've chosen {color(Fore.CYAN, platform)} as your platform.")
    safe_print()
    sleep(1)

    if platform == "ESP32":
        board_link = (
            "http://docs.platformio.org/en/latest/platforms/espressif32.html#boards"
        )
    elif platform == "ESP8266":
        board_link = (
            "http://docs.platformio.org/en/latest/platforms/espressif8266.html#boards"
        )
    elif platform == "RP2040":
        board_link = (
            "https://www.raspberrypi.com/documentation/microcontrollers/rp2040.html"
        )
    elif platform in ["BK72XX", "RTL87XX"]:
        board_link = "https://docs.libretiny.eu/docs/status/supported/"
    else:
        raise NotImplementedError("Unknown platform!")

    safe_print(f"Next, I need to know what {color(Fore.GREEN, 'board')} you're using.")
    sleep(0.5)
    safe_print(f"Please go to {color(Fore.GREEN, board_link)} and choose a board.")
    if platform == "ESP32":
        safe_print(f"(Type {color(Fore.GREEN, 'esp01_1m')} for Sonoff devices)")
    safe_print()
    # Don't sleep because user needs to copy link
    if platform == "ESP32":
        safe_print(f"For example \"{color(Fore.BOLD_WHITE, 'nodemcu-32s')}\".")
        boards_list = esp32_boards.BOARDS.items()
    elif platform == "ESP8266":
        safe_print(f"For example \"{color(Fore.BOLD_WHITE, 'nodemcuv2')}\".")
        boards_list = esp8266_boards.BOARDS.items()
    elif platform == "BK72XX":
        safe_print(f"For example \"{color(Fore.BOLD_WHITE, 'cb2s')}\".")
        boards_list = bk72xx_boards.BOARDS.items()
    elif platform == "RTL87XX":
        safe_print(f"For example \"{color(Fore.BOLD_WHITE, 'wr3')}\".")
        boards_list = rtl87xx_boards.BOARDS.items()
    elif platform == "RP2040":
        safe_print(f"For example \"{color(Fore.BOLD_WHITE, 'rpipicow')}\".")
        boards_list = rp2040_boards.BOARDS.items()

    else:
        raise NotImplementedError("Unknown platform!")

    boards = []
    safe_print("Options:")
    for board_id, board_data in boards_list:
        safe_print(f" - {board_id} - {board_data['name']}")
        boards.append(board_id)

    while True:
        board = safe_input(color(Fore.BOLD_WHITE, "(board): "))
        try:
            board = vol.All(vol.Lower, vol.Any(*boards))(board)
            break
        except vol.Invalid:
            safe_print(
                color(Fore.RED, f'Sorry, I don\'t think the board "{board}" exists.')
            )
            safe_print()
            sleep(0.25)
            safe_print()

    safe_print(f"Way to go! You've chosen {color(Fore.CYAN, board)} as your board.")
    safe_print()
    sleep(1)

    # Do not create wifi if the board does not support it
    if board not in ["rpipico"]:
        safe_print_step(3, WIFI_BIG)
        safe_print("In this step, I'm going to create the configuration for WiFi.")
        safe_print()
        sleep(1)
        safe_print(
            f"First, what's the {color(Fore.GREEN, 'SSID')} (the name) of the WiFi network {name} should connect to?"
        )
        sleep(1.5)
        safe_print(f"For example \"{color(Fore.BOLD_WHITE, 'Abraham Linksys')}\".")
        while True:
            ssid = safe_input(color(Fore.BOLD_WHITE, "(ssid): "))
            try:
                ssid = cv.ssid(ssid)
                break
            except vol.Invalid:
                safe_print(
                    color(
                        Fore.RED,
                        f'Unfortunately, "{ssid}" doesn\'t seem to be a valid SSID. Please try again.',
                    )
                )
                safe_print()
                sleep(1)

        safe_print(
            f'Thank you very much! You\'ve just chosen "{color(Fore.CYAN, ssid)}" as your SSID.'
        )
        safe_print()
        sleep(0.75)

        safe_print(
            f"Now please state the {color(Fore.GREEN, 'password')} of the WiFi network so that I can connect to it (Leave empty for no password)"
        )
        safe_print()
        safe_print(f"For example \"{color(Fore.BOLD_WHITE, 'PASSWORD42')}\"")
        sleep(0.5)
        psk = safe_input(color(Fore.BOLD_WHITE, "(PSK): "))
        safe_print(
            "Perfect! WiFi is now set up (you can create static IPs and so on later)."
        )
        sleep(1.5)

        safe_print_step(4, OTA_BIG)
        safe_print(
            "Almost there! ESPHome can automatically upload custom firmwares over WiFi "
            "(over the air) and integrates into Home Assistant with a native API."
        )
        safe_print(
            f"This can be insecure if you do not trust the WiFi network. Do you want to set a {color(Fore.GREEN, 'password')} for connecting to this ESP?"
        )
        safe_print()
        sleep(0.25)
        safe_print("Press ENTER for no password")
        password = safe_input(color(Fore.BOLD_WHITE, "(password): "))
    else:
        ssid, password, psk = "", "", ""

    if not wizard_write(
        path=path,
        name=name,
        platform=platform,
        board=board,
        ssid=ssid,
        psk=psk,
        password=password,
    ):
        return 1

    safe_print()
    safe_print(
        color(Fore.CYAN, "DONE! I've now written a new configuration file to ")
        + color(Fore.BOLD_CYAN, path)
    )
    safe_print()
    safe_print("Next steps:")
    safe_print("  > Follow the rest of the getting started guide:")
    safe_print(
        "  > https://esphome.io/guides/getting_started_command_line.html#adding-some-features"
    )
    safe_print("  > to learn how to customize ESPHome and install it to your device.")
    return 0
