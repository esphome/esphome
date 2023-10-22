### ota_http

ota in client (http) mode: This allow to do a remote ota when the device is far away, behind a firewall, and only reachable with mqtt. (For ex in an other house).



```yaml
external_components:
  - source: github://oarcher/piotech/


ota_http:


button:
  - platform: template
    name: "Firmware update"
    on_press:
      then:
        - ota_http.flash:
            url: http://example.com/firmware.bin
        - logger.log: "This message should be not displayed(reboot)"
```

The file `firmware.bin` can be found at `.esphome/build/xxxx/.pioenvs/xxx/firmware.bin` if esphome CLI is used, or downloaded as `Legacy format` from the esphome HA addon. Do not use `firmware-factory.bin` or `Modern format`.

You should got in the logs:
```
[18:48:30][D][button:010]: 'Firmware update' Pressed.
[18:48:30][D][ota_http:079]: Trying to connect to http://rasp:8080/firmware.bin
[18:48:30][D][ota_http:042]: Using ArduinoESP32OTABackend
[18:48:30][D][ota_http:209]: Progress: 0.1%
...
[18:48:39][D][ota_http:209]: Progress: 100.0%
[18:48:39][D][ota_http:218]: Done in 100 secs
[18:48:39][D][ota_http:223]: md5sum recieved: 38e2cad1c79fb38583361a41b9d16d27 (size 1378112)
[18:48:39][I][ota_http:242]: OTA update finished! Rebooting...
```
