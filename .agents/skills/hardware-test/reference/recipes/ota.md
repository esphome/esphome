# ota

## OTA overlap and lock (#20588), OTA timeouts (#19041), deflate (#19037)

All overlap orders (a web upload during a native OTA and the reverse) must be refused; the lock release
after an interrupted web upload waits about 14 s (ESP8266) or 7 s (ESP-IDF) for disconnect detection. A
refused web client on `web_server_idf` never gets the busy reply; the connection drops at reboot.
`pkill -f "a\|b"` does not alternate; use two pkills.
