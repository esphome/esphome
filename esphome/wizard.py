from __future__ import print_function

import codecs
import os
import random
import string
import unicodedata

import voluptuous as vol

import esphome.config_validation as cv
from esphome.helpers import color, get_bool_env
# pylint: disable=anomalous-backslash-in-string
from esphome.pins import ESP32_BOARD_PINS, ESP8266_BOARD_PINS
from esphome.py_compat import safe_input, text_type
from esphome.storage_json import StorageJSON, ext_storage_path
from esphome.util import safe_print

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

BASE_CONFIG = u"""esphome:
  name: {name}
  platform: {platform}
  board: {board}

wifi:
  ssid: "{ssid}"
  password: "{psk}"

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "{fallback_name}"
    password: "{fallback_psk}"

captive_portal:

# Enable logging
logger:

# Enable Home Assistant API
api:
"""


def sanitize_double_quotes(value):
    return value.replace('\\', '\\\\').replace('"', '\\"')


def wizard_file(**kwargs):
    letters = string.ascii_letters + string.digits
    ap_name_base = kwargs['name'].replace('_', ' ').title()
    ap_name = "{} Fallback Hotspot".format(ap_name_base)
    if len(ap_name) > 32:
        ap_name = ap_name_base
    kwargs['fallback_name'] = ap_name
    kwargs['fallback_psk'] = ''.join(random.choice(letters) for _ in range(12))

    config = BASE_CONFIG.format(**kwargs)

    if kwargs['password']:
        config += u'  password: "{0}"\n\nota:\n  password: "{0}"\n'.format(kwargs['password'])
    else:
        config += u"\nota:\n"

    return config


def wizard_write(path, **kwargs):
    name = kwargs['name']
    board = kwargs['board']

    kwargs['ssid'] = sanitize_double_quotes(kwargs['ssid'])
    kwargs['psk'] = sanitize_double_quotes(kwargs['psk'])
    kwargs['password'] = sanitize_double_quotes(kwargs['password'])

    if 'platform' not in kwargs:
        kwargs['platform'] = 'ESP8266' if board in ESP8266_BOARD_PINS else 'ESP32'
    platform = kwargs['platform']

    with codecs.open(path, 'w', 'utf-8') as f_handle:
        f_handle.write(wizard_file(**kwargs))
    storage = StorageJSON.from_wizard(name, name + '.local', platform, board)
    storage_path = ext_storage_path(os.path.dirname(path), os.path.basename(path))
    storage.save(storage_path)


if get_bool_env('ESPHOME_QUICKWIZARD'):
    def sleep(time):
        pass
else:
    from time import sleep


def safe_print_step(step, big):
    safe_print()
    safe_print()
    safe_print("============= STEP {} =============".format(step))
    safe_print(big)
    safe_print("===================================")
    sleep(0.25)


def default_input(text, default):
    safe_print()
    safe_print(u"Press ENTER for default ({})".format(default))
    return safe_input(text.format(default)) or default


# From https://stackoverflow.com/a/518232/8924614
def strip_accents(value):
    return u''.join(c for c in unicodedata.normalize('NFD', text_type(value))
                    if unicodedata.category(c) != 'Mn')


def wizard(path):
    if not path.endswith('.yaml') and not path.endswith('.yml'):
        safe_print(u"Please make your configuration file {} have the extension .yaml or .yml"
                   u"".format(color('cyan', path)))
        return 1
    if os.path.exists(path):
        safe_print(u"Uh oh, it seems like {} already exists, please delete that file first "
                   u"or chose another configuration file.".format(color('cyan', path)))
        return 1
    safe_print("Hi there!")
    sleep(1.5)
    safe_print("I'm the wizard of ESPHome :)")
    sleep(1.25)
    safe_print("And I'm here to help you get started with ESPHome.")
    sleep(2.0)
    safe_print("In 4 steps I'm going to guide you through creating a basic "
               "configuration file for your custom ESP8266/ESP32 firmware. Yay!")
    sleep(3.0)
    safe_print()
    safe_print_step(1, CORE_BIG)
    safe_print("First up, please choose a " + color('green', 'name') + " for your node.")
    safe_print("It should be a unique name that can be used to identify the device later.")
    sleep(1)
    safe_print("For example, I like calling the node in my living room {}.".format(
        color('bold_white', "livingroom")))
    safe_print()
    sleep(1)
    name = safe_input(color("bold_white", "(name): "))
    while True:
        try:
            name = cv.valid_name(name)
            break
        except vol.Invalid:
            safe_print(color("red", u"Oh noes, \"{}\" isn't a valid name. Names can only include "
                                    u"numbers, letters and underscores.".format(name)))
            name = strip_accents(name).replace(' ', '_')
            name = u''.join(c for c in name if c in cv.ALLOWED_NAME_CHARS)
            safe_print(u"Shall I use \"{}\" as the name instead?".format(color('cyan', name)))
            sleep(0.5)
            name = default_input(u"(name [{}]): ", name)

    safe_print(u"Great! Your node is now called \"{}\".".format(color('cyan', name)))
    sleep(1)
    safe_print_step(2, ESP_BIG)
    safe_print("Now I'd like to know what microcontroller you're using so that I can compile "
               "firmwares for it.")
    safe_print("Are you using an " + color('green', 'ESP32') + " or " +
               color('green', 'ESP8266') + " platform? (Choose ESP8266 for Sonoff devices)")
    while True:
        sleep(0.5)
        safe_print()
        safe_print("Please enter either ESP32 or ESP8266.")
        platform = safe_input(color("bold_white", "(ESP32/ESP8266): "))
        try:
            platform = vol.All(vol.Upper, vol.Any('ESP32', 'ESP8266'))(platform)
            break
        except vol.Invalid:
            safe_print(u"Unfortunately, I can't find an espressif microcontroller called "
                       u"\"{}\". Please try again.".format(platform))
    safe_print(u"Thanks! You've chosen {} as your platform.".format(color('cyan', platform)))
    safe_print()
    sleep(1)

    if platform == 'ESP32':
        board_link = 'http://docs.platformio.org/en/latest/platforms/espressif32.html#boards'
    else:
        board_link = 'http://docs.platformio.org/en/latest/platforms/espressif8266.html#boards'

    safe_print("Next, I need to know what " + color('green', 'board') + " you're using.")
    sleep(0.5)
    safe_print("Please go to {} and choose a board.".format(color('green', board_link)))
    if platform == 'ESP32':
        safe_print("(Type " + color('green', 'esp01_1m') + " for Sonoff devices)")
    safe_print()
    # Don't sleep because user needs to copy link
    if platform == 'ESP32':
        safe_print("For example \"{}\".".format(color("bold_white", 'nodemcu-32s')))
        boards = list(ESP32_BOARD_PINS.keys())
    else:
        safe_print("For example \"{}\".".format(color("bold_white", 'nodemcuv2')))
        boards = list(ESP8266_BOARD_PINS.keys())
    safe_print("Options: {}".format(', '.join(sorted(boards))))

    while True:
        board = safe_input(color("bold_white", "(board): "))
        try:
            board = vol.All(vol.Lower, vol.Any(*boards))(board)
            break
        except vol.Invalid:
            safe_print(color('red', "Sorry, I don't think the board \"{}\" exists.".format(board)))
            safe_print()
            sleep(0.25)
            safe_print()

    safe_print(u"Way to go! You've chosen {} as your board.".format(color('cyan', board)))
    safe_print()
    sleep(1)

    safe_print_step(3, WIFI_BIG)
    safe_print("In this step, I'm going to create the configuration for "
               "WiFi.")
    safe_print()
    sleep(1)
    safe_print("First, what's the " + color('green', 'SSID') +
               u" (the name) of the WiFi network {} I should connect to?".format(name))
    sleep(1.5)
    safe_print("For example \"{}\".".format(color('bold_white', "Abraham Linksys")))
    while True:
        ssid = safe_input(color('bold_white', "(ssid): "))
        try:
            ssid = cv.ssid(ssid)
            break
        except vol.Invalid:
            safe_print(color('red', u"Unfortunately, \"{}\" doesn't seem to be a valid SSID. "
                                    u"Please try again.".format(ssid)))
            safe_print()
            sleep(1)

    safe_print(u"Thank you very much! You've just chosen \"{}\" as your SSID."
               u"".format(color('cyan', ssid)))
    safe_print()
    sleep(0.75)

    safe_print("Now please state the " + color('green', 'password') +
               " of the WiFi network so that I can connect to it (Leave empty for no password)")
    safe_print()
    safe_print("For example \"{}\"".format(color('bold_white', 'PASSWORD42')))
    sleep(0.5)
    psk = safe_input(color('bold_white', '(PSK): '))
    safe_print("Perfect! WiFi is now set up (you can create static IPs and so on later).")
    sleep(1.5)

    safe_print_step(4, OTA_BIG)
    safe_print("Almost there! ESPHome can automatically upload custom firmwares over WiFi "
               "(over the air) and integrates into Home Assistant with a native API.")
    safe_print("This can be insecure if you do not trust the WiFi network. Do you want to set "
               "a " + color('green', 'password') + " for connecting to this ESP?")
    safe_print()
    sleep(0.25)
    safe_print("Press ENTER for no password")
    password = safe_input(color('bold_white', '(password): '))

    wizard_write(path=path, name=name, platform=platform, board=board,
                 ssid=ssid, psk=psk, password=password)

    safe_print()
    safe_print(color('cyan', "DONE! I've now written a new configuration file to ") +
               color('bold_cyan', path))
    safe_print()
    safe_print("Next steps:")
    safe_print("  > Check your Home Assistant \"integrations\" screen. If all goes well, you "
               "should see your ESP being discovered automatically.")
    safe_print("  > Then follow the rest of the getting started guide:")
    safe_print("  > https://esphome.io/guides/getting_started_command_line.html")
    return 0
