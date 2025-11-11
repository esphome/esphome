---
description: "Migration guide for installing ESPHome on ESPs running ESPurna."
title: "Migrating from ESPurna"
params:
  seo:
    description: Migration guide for installing ESPHome on ESPs running ESPurna.
    image: espurna.svg
---

Migrating from previous ESPurna setups is very easy. You just need to have
ESPHome create a binary for you and then upload that in the ESPurna web interface.

## Getting Binary

First follow the guides for the [different supported devices](/components#devices) and create a configuration
file. Then, generate and download the binary:

- **Using the Home Assistant add-on/dashboard**: Just click the `COMPILE`
  button, wait for the compilation to end and press the `DOWNLOAD BINARY`
  button.

{{< img src="download_binary.png" alt="Image" >}}

- **Using the command line**: run `esphome compile livingroom.yaml` (replacing
  `livingroom.yaml` with your configuration file of course) and navigate to the
  `<NODE_NAME>/.pioenvs/<NODE_NAME>/` folder. There you will find a `firmware.bin` file,
  this is the binary you will upload.

## Uploading Binary

To upload the binary, navigate to the ESPurna web interface and enter the
"General " section.

{{< img src="espurna_ota.png" alt="Image" width="80.0%" class="align-center" >}}

In the "Upgrade" section, choose the binary you previously downloaded and press "Upgrade".
If everything succeeds, you should now have ESPHome on your node 🎉

> [!NOTE]
> with ESPHome, you in most cases won't need to worry about the available flash size, as
> the binary only ever includes the code that you are actually using.

{{< img src="espurna_upload.png" alt="Image" width="90.0%" class="align-center" >}}

Happy Hacking!

## See Also

- {{< docref "/components/esp8266" >}}
- {{< docref "/components/esp32" >}}
- {{< docref "migrate_espeasy/" >}}
- {{< docref "migrate_sonoff_tasmota/" >}}
