---
description: "Instructions for setting up a NFC binary sensor in ESPHome"
title: "NFC Binary Sensor"
params:
  seo:
    description: Instructions for setting up a NFC binary sensor in ESPHome
    image: nfc.png
---

{{< anchor "nfc-platform" >}}

The `nfc` binary sensor platform provides an easy way for you to determine if an NFC tag is presented to the reader.
The tag may be identified in one of three ways:

- By given unique ID (`uid`  ) -- for example `74-10-37-94`
- By a given NDEF tag "name", or...
- By a given string contained in the tag's NDEF message/data

Note that this platform is currently supported by the {{< docref "../pn7150" "PN7150" >}} and {{< docref "../pn7160" "PN716x" >}} only;
one of these components must be present in your device's configuration in order to use it.

```yaml
# Example configuration entries
binary_sensor:
  - platform: nfc
    ndef_contains: pulse
    name: "NFC 1 Tag"
  - platform: nfc
    tag_id: pulsed
    name: "NFC 2 Tag"
  - platform: nfc
    uid: 74-10-37-94
    name: "MFC Tag"
```

## Configuration variables

- **ndef_contains** (*Optional*, string): A (sub)string that must appear in the tag's NDEF message. May not be used
  with `tag_id` and/or `uid` (below).

- **tag_id** (*Optional*, string): A string that identifies the tag; in effect, its name. Specifically, this looks
  for the Home Assistant URI encoded into one of the tag's NDEF records and then looks for this specific string. May
  not be used with `ndef_contains` and/or `uid`.

- **uid** (*Optional*, string): The unique ID of the NFC tag. This is a hyphen-separated list of hexadecimal values.
  For example: `74-10-37-94`. May not be used with `ndef_contains` and/or `tag_id` (above).

- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

{{< anchor "nfc-setting_up_tags" >}}

## Setting Up Tags

To set up a binary sensor for a given NFC tag, you must first know either its unique ID (`uid`  ), tag ID (if it was
prepared using the Home Assistant Companion app) or (part of) a string that is contained within its NDEF message.

To obtain a tag's UID:

- Set up a simple NFC component (such as the {{< docref "pn532" "PN532" >}}, {{< docref "../pn7150" "PN7150" >}} or {{< docref "../pn7160" "PN716x" >}})
  configuration without any binary sensors.

- Approach the NFC reader with an NFC tag. When the tag is sufficiently close to the reader, you'll see a message in the
  ESPHome device's logs similar to this:

  ```log
  Read tag type Mifare Classic with UID 1C-E5-E7-A6
  ```

- Either:

  - Copy this ID and use it to create a `binary_sensor` entry as shown in the configuration example above, or...
  - Use the tag ID (as determined when it was prepared with the Home Assistant Companion app) to define the `tag_id`
    parameter for the `binary_sensor` as shown above, or...

  - Choose a substring contained within the tag's NDEF message and use this to define the `ndef_contains` parameter
    as shown in the example above. If present, the tag's NDEF records will appear in the log on the lines just below
    the message shown above.

Repeat this process for each tag.

Note that, since *you* are able to define the NDEF message, this approach is more flexible and even allows multiple
cards/tags to share the same message.

## See Also

- {{< docref "index/" >}}
- {{< docref "pn532/" >}}
- {{< docref "../pn7150" >}}
- {{< docref "../pn7160" >}}
- {{< apiref "pn532/pn532.h" "pn532/pn532.h" >}}
- {{< apiref "pn7150/pn7150.h" "pn7150/pn7150.h" >}}
- {{< apiref "pn7160/pn7160.h" "pn7160/pn7160.h" >}}
