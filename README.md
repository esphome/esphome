# esphomeyaml for [esphomelib](https://github.com/OttoWinter/esphomelib)

### Getting Started Guide: https://esphomelib.com/esphomeyaml/guides/getting_started_command_line.html

### Available Components: https://esphomelib.com/esphomeyaml/index.html

esphomeyaml is the solution for your ESP8266/ESP32 projects with Home Assistant. It allows you to create **custom firmwares** for your microcontrollers with no programming experience required. All you need to know is the YAML configuration format which is also used by [Home Assistant](https://www.home-assistant.io).

esphomeyaml will:

 * Read your configuration file and warn you about potential errors (like using the invalid pins.)
 * Create a custom C++ sketch file for you using esphomeyaml's powerful C++ generation engine.
 * Compile the sketch file for you using [platformio](http://platformio.org/).
 * Upload the binary to your ESP via Over the Air updates.
 * Automatically start remote logs via MQTT.

And all of that with a single command 🎉:

```bash
esphomeyaml configuration.yaml run
```

## Features

 * **No programming experience required:** just edit YAML configuration
    files like you're used to with Home Assistant.
 * **Flexible:** Use [esphomelib](https://github.com/OttoWinter/esphomelib)'s powerful core to create custom sensors/outputs.
 * **Fast and efficient:** Written in C++ and keeps memory consumption to a minimum.
 * **Made for [Home Assistant](https://www.home-assistant.io):** Almost all [Home Assistant](https://www.home-assistant.io) features are supported out of the box. Including RGB lights and many more.
 * **Easy reproducible configuration:** No need to go through a long setup process for every single node. Just copy a configuration file and run a single command.
 * **Smart Over The Air Updates:** esphomeyaml has OTA updates deeply integrated into the system. It even automatically enters a recovery mode if a boot loop is detected.
 * **Powerful logging engine:** View colorful logs and debug issues remotely.
 * **Open Source**
 * For me: Makes documenting esphomelib's features a lot easier.

## Special Thanks

Special Thanks to the Home Assistant project. Lots of the code base of esphomeyaml is based off of Home Assistant, for example the loading and config validation code.
