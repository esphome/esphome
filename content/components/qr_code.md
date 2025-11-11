---
description: "Instructions for displaying a QR Code in ESPHome"
title: "QR Code Component"
params:
  seo:
    description: Instructions for displaying a QR Code in ESPHome
    image: qr-code.svg
---

{{< anchor "display-qrcode" >}}

Use this component to generate a QR-code containing a string on the device, which can then be drawn on compatible displays.

```yaml
qr_code:
  - id: homepage_qr
    value: esphome.io
```

## Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID with which you will be able to reference the QR-code later
  in your display code.

- **value** (**Required**, string): The string which you want to encode in the QR-code.
- **ecc** (*Optional*, string): The error correction code level you want to use. Defaults to `LOW`. You can use one of
  the following values:

  - `LOW`  : The QR Code can tolerate about 7% erroneous codewords
  - `MEDIUM`  : The QR Code can tolerate about 15% erroneous codewords
  - `QUARTILE`  : The QR Code can tolerate about 25% erroneous codewords
  - `HIGH`  : The QR Code can tolerate about 30% erroneous codewords

To draw the QR-code, call the `it.qr_code` function from your render lambda:

```yaml
display:
  - platform: ...
    # ...
    pages:
      - id: page1
        lambda: |-
          // Draw the QR-code at position [x=50,y=0] with white color and a 2x scale
          it.qr_code(50, 0, id(homepage_qr), Color(255,255,255), 2);

          // Draw the QR-code in the center of the screen with white color and a 2x scale
          auto size = id(homepage_qr).get_size() * 2; // Multiply by scale
          auto x = (it.get_width() / 2) - (size / 2);
          auto y = (it.get_height() / 2) - (size / 2);
          it.qr_code(x, y, id(homepage_qr), Color(255,255,255), 2);
```
