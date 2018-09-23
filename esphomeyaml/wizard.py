from __future__ import print_function

import codecs
import os
import unicodedata

import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import mqtt
from esphomeyaml.const import ESP_PLATFORMS, ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266
from esphomeyaml.helpers import color


# pylint: disable=anomalous-backslash-in-string
from esphomeyaml.pins import ESP32_BOARD_PINS, ESP8266_BOARD_PINS

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
BASE_CONFIG = """esphomeyaml:
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
        config += "ota:\n  password: '{}'\n".format(kwargs['ota_password'])
    else:
        config += "ota:\n"

    return config


if os.getenv('ESPHOMEYAML_QUICKWIZARD', False):
    def sleep(time):
        pass
else:
    from time import sleep


def print_step(step, big):
    print()
    print()
    print("============= STEP {} =============".format(step))
    print(big)
    print("===================================")
    sleep(0.25)


def default_input(text, default):
    print()
    print("Press ENTER for default ({})".format(default))
    return raw_input(text.format(default)) or default


# From https://stackoverflow.com/a/518232/8924614
def strip_accents(string):
    return u''.join(c for c in unicodedata.normalize('NFD', unicode(string))
                    if unicodedata.category(c) != 'Mn')


def wizard(path):
    if not path.endswith('.yaml') and not path.endswith('.yml'):
        print("Please make your configuration file {} have the extension .yaml or .yml".format(
            color('cyan', path)))
        return 1
    if os.path.exists(path):
        print("Uh oh, it seems like {} already exists, please delete that file first "
              "or chose another configuration file.".format(color('cyan', path)))
        return 1
    print("Hi there!")
    sleep(1.5)
    print("I'm the wizard of esphomeyaml :)")
    sleep(1.25)
    print("And I'm here to help you get started with esphomeyaml.")
    sleep(2.0)
    print("In 5 steps I'm going to guide you through creating a basic "
          "configuration file for your custom ESP8266/ESP32 firmware. Yay!")
    sleep(3.0)
    print()
    print_step(1, CORE_BIG)
    print("First up, please choose a " + color('green', 'name') + " for your node.")
    print("It should be a unique name that can be used to identify the device later.")
    sleep(1)
    print("For example, I like calling the node in my living room {}.".format(
        color('bold_white', "livingroom")))
    print()
    sleep(1)
    name = raw_input(color("bold_white", "(name): "))
    while True:
        try:
            name = cv.valid_name(name)
            break
        except vol.Invalid:
            print(color("red", "Oh noes, \"{}\" isn't a valid name. Names can only include "
                               "numbers, letters and underscores.".format(name)))
            name = strip_accents(name).replace(' ', '_')
            name = u''.join(c for c in name if c in cv.ALLOWED_NAME_CHARS)
            print("Shall I use \"{}\" as the name instead?".format(color('cyan', name)))
            sleep(0.5)
            name = default_input("(name [{}]): ", name)

    print("Great! Your node is now called \"{}\".".format(color('cyan', name)))
    sleep(1)
    print_step(2, ESP_BIG)
    print("Now I'd like to know what microcontroller you're using so that I can compile "
          "firmwares for it.")
    print("Are you using an " + color('green', 'ESP32') + " or " +
          color('green', 'ESP8266') + " platform? (Choose ESP8266 for Sonoff devices)")
    while True:
        sleep(0.5)
        print()
        print("Please enter either ESP32 or ESP8266.")
        platform = raw_input(color("bold_white", "(ESP32/ESP8266): "))
        try:
            platform = vol.All(vol.Upper, vol.Any(*ESP_PLATFORMS))(platform)
            break
        except vol.Invalid:
            print("Unfortunately, I can't find an espressif microcontroller called "
                  "\"{}\". Please try again.".format(platform))
    print("Thanks! You've chosen {} as your platform.".format(color('cyan', platform)))
    print()
    sleep(1)

    if platform == ESP_PLATFORM_ESP32:
        board_link = 'http://docs.platformio.org/en/latest/platforms/espressif32.html#boards'
    else:
        board_link = 'http://docs.platformio.org/en/latest/platforms/espressif8266.html#boards'

    print("Next, I need to know what " + color('green', 'board') + " you're using.")
    sleep(0.5)
    print("Please go to {} and choose a board.".format(color('green', board_link)))
    if platform == ESP_PLATFORM_ESP8266:
        print("(Type " + color('green', 'esp01_1m') + " for Sonoff devices)")
    print()
    # Don't sleep because user needs to copy link
    if platform == ESP_PLATFORM_ESP32:
        print("For example \"{}\".".format(color("bold_white", 'nodemcu-32s')))
        boards = list(ESP32_BOARD_PINS.keys())
    else:
        print("For example \"{}\".".format(color("bold_white", 'nodemcuv2')))
        boards = list(ESP8266_BOARD_PINS.keys())
    while True:
        board = raw_input(color("bold_white", "(board): "))
        try:
            board = vol.All(vol.Lower, vol.Any(*boards))(board)
            break
        except vol.Invalid:
            print(color('red', "Sorry, I don't think the board \"{}\" exists."))
            print()
            sleep(0.25)
            print("Possible options are {}".format(', '.join(boards)))
            print()

    print("Way to go! You've chosen {} as your board.".format(color('cyan', board)))
    print()
    sleep(1)

    print_step(3, WIFI_BIG)
    print("In this step, I'm going to create the configuration for "
          "WiFi.")
    print()
    sleep(1)
    print("First, what's the " + color('green', 'SSID') + " (the name) of the WiFi network {} "
                                                          "I should connect to?".format(name))
    sleep(1.5)
    print("For example \"{}\".".format(color('bold_white', "Abraham Linksys")))
    while True:
        ssid = raw_input(color('bold_white', "(ssid): "))
        try:
            ssid = cv.ssid(ssid)
            break
        except vol.Invalid:
            print(color('red', "Unfortunately, \"{}\" doesn't seem to be a valid SSID. "
                               "Please try again.".format(ssid)))
            print()
            sleep(1)

    print("Thank you very much! You've just chosen \"{}\" as your SSID.".format(
        color('cyan', ssid)))
    print()
    sleep(0.75)

    print("Now please state the " + color('green', 'password') +
          " of the WiFi network so that I can connect to it (Leave empty for no password)")
    print()
    print("For example \"{}\"".format(color('bold_white', 'PASSWORD42')))
    sleep(0.5)
    psk = raw_input(color('bold_white', '(PSK): '))
    print("Perfect! WiFi is now set up (you can create static IPs and so on later).")
    sleep(1.5)

    print_step(4, MQTT_BIG)
    print("Almost there! Now let's setup MQTT so that your node can connect to the outside world.")
    print()
    sleep(1)
    print("Please enter the " + color('green', 'address') + " of your MQTT broker.")
    print()
    print("For example \"{}\".".format(color('bold_white', '192.168.178.84')))
    while True:
        broker = raw_input(color('bold_white', "(broker): "))
        try:
            broker = mqtt.validate_broker(broker)
            break
        except vol.Invalid as err:
            print(color('red', "The broker address \"{}\" seems to be invalid: {} :(".format(
                broker, err)))
            print("Please try again.")
            print()
            sleep(1)

    print("Thanks! Now enter the " + color('green', 'username') + " and " +
          color('green', 'password') + " for the MQTT broker. Leave empty for no authentication.")
    mqtt_username = raw_input(color('bold_white', '(username): '))
    mqtt_password = ''
    if mqtt_username:
        mqtt_password = raw_input(color('bold_white', '(password): '))

        show = '*' * len(mqtt_password)
        if len(mqtt_password) >= 2:
            show = mqtt_password[:2] + '*' * len(mqtt_password)
        print("MQTT Username: \"{}\"; Password: \"{}\"".format(
            color('cyan', mqtt_username), color('cyan', show)))
    else:
        print("No authentication for MQTT")

    print_step(5, OTA_BIG)
    print("Last step! esphomeyaml can automatically upload custom firmwares over WiFi "
          "(over the air).")
    print("This can be insecure if you do not trust the WiFi network. Do you want to set "
          "an " + color('green', 'OTA password') + " for remote updates?")
    print()
    sleep(0.25)
    print("Press ENTER for no password")
    ota_password = raw_input(color('bold_white', '(password): '))

    config = wizard_file(name=name, platform=platform, board=board,
                         ssid=ssid, psk=psk, broker=broker,
                         mqtt_username=mqtt_username, mqtt_password=mqtt_password,
                         ota_password=ota_password)

    with codecs.open(path, 'w') as f_handle:
        f_handle.write(config)

    print()
    print(color('cyan', "DONE! I've now written a new configuration file to ") +
          color('bold_cyan', path))
    print()
    print("Next steps:")
    print("  > If you haven't already, enable MQTT discovery in Home Assistant:")
    print()
    print(color('bold_white', "# In your configuration.yaml"))
    print(color('bold_white', "mqtt:"))
    print(color('bold_white', "  broker: {}".format(broker)))
    print(color('bold_white', "  # ..."))
    print(color('bold_white', "  discovery: True"))
    print()
    print("  > Then follow the rest of the getting started guide:")
    print("  > https://esphomelib.com/esphomeyaml/guides/getting_started_command_line.html")
    return 0
