# espnow

## ESP-NOW on the ESP8266 (#14407)

The NONOS SDK only delivers broadcast ESP-NOW frames while the station is associated or a soft-AP is
up; a hidden, open, client-less soft-AP (60 s beacon, DHCP stopped) makes broadcasts arrive. In AP-only
mode the device transmits from the soft-AP MAC.
