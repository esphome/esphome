# Esphomeyaml Hass.io Add-On

[![esphomeyaml logo](https://raw.githubusercontent.com/OttoWinter/esphomeyaml/dev/esphomeyaml-edge/logo.png)](https://esphomelib.com/esphomeyaml/index.html)

[![GitHub stars](https://img.shields.io/github/stars/OttoWinter/esphomelib.svg?style=social&label=Star&maxAge=2592000)](https://github.com/OttoWinter/esphomelib)
[![GitHub Release][releases-shield]][releases]
[![Discord][discord-shield]][discord]

## About

This add-on allows you to manage and program your ESP8266 and ESP32 based microcontrollers
directly through Hass.io **with no programming experience required**. All you need to do
is write YAML configuration files; the rest (over-the-air updates, compiling) is all
handled by esphomeyaml.

<p align="center">
<img title="esphomeyaml dashboard screenshot" src="https://raw.githubusercontent.com/OttoWinter/esphomeyaml/dev/esphomeyaml-edge/images/screenshot.png" width="700px"></img>
</p>

[_View the esphomeyaml documentation here_](https://esphomelib.com/esphomeyaml/index.html)

## Example

With esphomeyaml, you can go from a few lines of YAML straight to a custom-made
firmware. For example, to include a [DHT22][dht22].
temperature and humidity sensor, you just need to include 8 lines of YAML
in your configuration file:

<img title="esphomeyaml DHT configuration example" src="https://raw.githubusercontent.com/OttoWinter/esphomeyaml/dev/esphomeyaml-edge/images/dht-example.png" width="500px"></img>

Then just click UPLOAD and the sensor will magically appear in Home Assistant:

<img title="esphomelib Home Assistant MQTT discovery" src="https://raw.githubusercontent.com/OttoWinter/esphomeyaml/dev/esphomeyaml-edge/images/temperature-humidity.png" width="600px"></img>

## Installation

To install this Hass.io add-on you need to add the esphomeyaml add-on repository
first:

1. Add the epshomeyaml add-ons repository to your Hass.io instance. You can do this by navigating to the "Add-on Store" tab in the Hass.io panel and then entering https://github.com/OttoWinter/esphomeyaml in the "Add new repository by URL" field.
2. Now scroll down and select the "esphomeyaml" add-on.
3. Press install to download the add-on and unpack it on your machine. This can take some time.
4. Optional: If you're using SSL certificates and want to encrypt your communication to this add-on, please enter `true` into the `ssl` field and set the `fullchain` and `certfile` options accordingly.
5. Start the add-on, check the logs of the add-on to see if everything went well.
6. Click "OPEN WEB UI" to open the esphomeyaml dashboard. You will be asked for your Home Assistant credentials - esphomeyaml uses Hass.io's authentication system to log you in.

**NOTE**: Installation on RPis running in 64-bit mode is currently not possible. Please use the 32-bit variant of HassOS instead.

You can view the esphomeyaml docs here: https://esphomelib.com/esphomeyaml/index.html

## Configuration

**Note**: _Remember to restart the add-on when the configuration is changed._

Example add-on configuration:

```json
{
  "ssl": false,
  "certfile": "fullchain.pem",
  "keyfile": "privkey.pem"
}
```

### Option: `ssl`

Enables/Disables encrypted SSL (HTTPS) connections to the web server of this add-on. Set it to `true` to encrypt communications, `false` otherwise. Please note that if you set this to `true` you must also specify a `certfile` and `keyfile`.

### Option: `certfile`

The certificate file to use for SSL.

**Note**: _The file MUST be stored in `/ssl/`, which is the default for Hass.io_

### Option: `keyfile`

The private key file to use for SSL.

**Note**: _The file MUST be stored in `/ssl/`, which is the default for Hass.io_

### Option: `leave_front_door_open`

Adding this option to the add-on configuration allows you to disable
authentication by setting it to `true`.

[discord-shield]: https://img.shields.io/discord/429907082951524364.svg
[dht22]: https://esphomelib.com/esphomeyaml/components/sensor/dht.html
[discord]: https://discord.me/KhAMKrd
[releases-shield]: https://img.shields.io/github/release/OttoWinter/esphomeyaml.svg
[releases]: https://esphomelib.com/esphomeyaml/changelog/index.html
[repository]: https://github.com/OttoWinter/esphomeyaml
