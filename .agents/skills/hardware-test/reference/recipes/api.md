# api

## Noise handshake and OTA encryption on the ESP8266 (#19062, #19233)

Host side timing is useless on a weak link; use an on-device bench with the minimum of five runs over
two boots. Provision a key with `noise_encryption_set_key` through aioesphomeapi and upload with
`espota2.run_ota(host, 8266, None, bin, noise_psk=key)` when the key is not in the yaml.
