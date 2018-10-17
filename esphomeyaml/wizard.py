from __future__ import print_function

import codecs
import os
import unicodedata

import voluptuous as vol

from esphomeyaml.components import mqtt
import esphomeyaml.config_validation as cv
from esphomeyaml.const import ESP_PLATFORMS, ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266
from esphomeyaml.helpers import color
# pylint: disable=anomalous-backslash-in-string
from esphomeyaml.pins import ESP32_BOARD_PINS, ESP8266_BOARD_PINS
from esphomeyaml.util import safe_print

CORE_BIG = """    _____ ____  _____  ______
   / ____/ __ \|  __ \|  ____|
  | |   | |  | | |__) | |__
  | |   | |  | |  _  /|  __|
  | |___| |__| | | \ \| |____
   \_____\____/|_|  \_\______|
"""
ESP_BIG = """      ______  _____ _____
     |  ____|/ ____|  __ \\
     | |__  | (___ | |__) |
     |  __|  \___ \|  ___/
     | |____ ____) | |
     |______|_____/|_|
"""
WIFI_BIG = """   __          ___ ______ _
   \ \        / (_)  ____(_)
    \ \  /\  / / _| |__   _
     \ \/  \/ / | |  __| | |
      \  /\  /  | | |    | |
       \/  \/   |_|_|    |_|
"""
MQTT_BIG = """   __  __  ____ _______ _______
  |  \/  |/ __ \__   __|__   __|
  | \  / | |  | | | |     | |
  | |\/| | |  | | | |     | |
  | |  | | |__| | | |     | |
  |_|  |_|\___\_\ |_|     |_|
"""
OTA_BIG = """       ____ _______
      / __ \__   __|/\\
     | |  | | | |  /  \\
     | |  | | | | / /\ \\
     | |__| | | |/ ____ \\
      \____/  |_/_/    \_\\
"""

# TODO handle escaping
BASE_CONFIG = u"""esphomeyaml:
  name: {name}
  platform: {platform}
  board: {board}

wifi:
  ssid: '{ssid}'
  password: '{psk}'

mqtt:
  broker: '{broker}'
  username: '{mqtt_username}'
  password: '{mqtt_password}'

# Enable logging
logger:

"""


def wizard_file(**kwargs):
    config = BASE_CONFIG.format(**kwargs)

    if kwargs['ota_password']:
        config += u"ota:\n  password: '{}'\n".format(kwargs['ota_password'])
    else:
        config += u"ota:\n"

    return config


if os.getenv('ESPHOMEYAML_QUICKWIZARD', False):
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
    return raw_input(text.format(default)) or default


# From https://stackoverflow.com/a/518232/8924614
def strip_accents(string):
    return u''.join(c for c in unicodedata.normalize('NFD', unicode(string))
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
    safe_print("I'm the wizard of esphomeyaml :)")
    sleep(1.25)
    safe_print("And I'm here to help you get started with esphomeyaml.")
    sleep(2.0)
    safe_print("In 5 steps I'm going to guide you through creating a basic "
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
    name = raw_input(color("bold_white", "(name): "))
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
        platform = raw_input(color("bold_white", "(ESP32/ESP8266): "))
        try:
            platform = vol.All(vol.Upper, vol.Any(*ESP_PLATFORMS))(platform)
            break
        except vol.Invalid:
            safe_print(u"Unfortunately, I can't find an espressif microcontroller called "
                       u"\"{}\". Please try again.".format(platform))
    safe_print(u"Thanks! You've chosen {} as your platform.".format(color('cyan', platform)))
    safe_print()
    sleep(1)

    if platform == ESP_PLATFORM_ESP32:
        board_link = 'http://docs.platformio.org/en/latest/platforms/espressif32.html#boards'
    else:
        board_link = 'http://docs.platformio.org/en/latest/platforms/espressif8266.html#boards'

    safe_print("Next, I need to know what " + color('green', 'board') + " you're using.")
    sleep(0.5)
    safe_print("Please go to {} and choose a board.".format(color('green', board_link)))
    if platform == ESP_PLATFORM_ESP8266:
        safe_print("(Type " + color('green', 'esp01_1m') + " for Sonoff devices)")
    safe_print()
    # Don't sleep because user needs to copy link
    if platform == ESP_PLATFORM_ESP32:
        safe_print("For example \"{}\".".format(color("bold_white", 'nodemcu-32s')))
        boards = list(ESP32_BOARD_PINS.keys())
    else:
        safe_print("For example \"{}\".".format(color("bold_white", 'nodemcuv2')))
        boards = list(ESP8266_BOARD_PINS.keys())
    while True:
        board = raw_input(color("bold_white", "(board): "))
        try:
            board = vol.All(vol.Lower, vol.Any(*boards))(board)
            break
        except vol.Invalid:
            safe_print(color('red', "Sorry, I don't think the board \"{}\" exists."))
            safe_print()
            sleep(0.25)
            safe_print("Possible options are {}".format(', '.join(boards)))
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
        ssid = raw_input(color('bold_white', "(ssid): "))
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
    psk = raw_input(color('bold_white', '(PSK): '))
    safe_print("Perfect! WiFi is now set up (you can create static IPs and so on later).")
    sleep(1.5)

    safe_print_step(4, MQTT_BIG)
    safe_print("Almost there! Now let's setup MQTT so that your node can connect to the "
               "outside world.")
    safe_print()
    sleep(1)
    safe_print("Please enter the " + color('green', 'address') + " of your MQTT broker.")
    safe_print()
    safe_print("For example \"{}\".".format(color('bold_white', '192.168.178.84')))
    while True:
        broker = raw_input(color('bold_white', "(broker): "))
        try:
            broker = mqtt.validate_broker(broker)
            break
        except vol.Invalid as err:
            safe_print(color('red', u"The broker address \"{}\" seems to be invalid: {} :("
                                    u"".format(broker, err)))
            safe_print("Please try again.")
            safe_print()
            sleep(1)

    safe_print("Thanks! Now enter the " + color('green', 'username') + " and " +
               color('green', 'password') +
               " for the MQTT broker. Leave empty for no authentication.")
    mqtt_username = raw_input(color('bold_white', '(username): '))
    mqtt_password = ''
    if mqtt_username:
        mqtt_password = raw_input(color('bold_white', '(password): '))

        show = '*' * len(mqtt_password)
        if len(mqtt_password) >= 2:
            show = mqtt_password[:2] + '*' * len(mqtt_password)
        safe_print(u"MQTT Username: \"{}\"; Password: \"{}\""
                   u"".format(color('cyan', mqtt_username), color('cyan', show)))
    else:
        safe_print("No authentication for MQTT")

    safe_print_step(5, OTA_BIG)
    safe_print("Last step! esphomeyaml can automatically upload custom firmwares over WiFi "
               "(over the air).")
    safe_print("This can be insecure if you do not trust the WiFi network. Do you want to set "
               "an " + color('green', 'OTA password') + " for remote updates?")
    safe_print()
    sleep(0.25)
    safe_print("Press ENTER for no password")
    ota_password = raw_input(color('bold_white', '(password): '))

    config = wizard_file(name=name, platform=platform, board=board,
                         ssid=ssid, psk=psk, broker=broker,
                         mqtt_username=mqtt_username, mqtt_password=mqtt_password,
                         ota_password=ota_password)

    with codecs.open(path, 'w') as f_handle:
        f_handle.write(config)

    safe_print()
    safe_print(color('cyan', "DONE! I've now written a new configuration file to ") +
               color('bold_cyan', path))
    safe_print()
    safe_print("Next steps:")
    safe_print("  > If you haven't already, enable MQTT discovery in Home Assistant:")
    safe_print()
    safe_print(color('bold_white', "# In your configuration.yaml"))
    safe_print(color('bold_white', "mqtt:"))
    safe_print(color('bold_white', u"  broker: {}".format(broker)))
    safe_print(color('bold_white', "  # ..."))
    safe_print(color('bold_white', "  discovery: True"))
    safe_print()
    safe_print("  > Then follow the rest of the getting started guide:")
    safe_print("  > https://esphomelib.com/esphomeyaml/guides/getting_started_command_line.html")
    return 0
