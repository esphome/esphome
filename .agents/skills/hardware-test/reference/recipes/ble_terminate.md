# BLE terminate with peripheral latency (#20232)

`scripts/ble_terminate`: an ESP32-S3 `bluetooth_proxy` (active) over Ethernet as the central (`btproxy.yaml`) and a
classic devkit `esp32_ble_server` peer (`btpeer.yaml`). The peer asks for 50 ms / latency 10 / 6 s with
`esp_ble_gap_update_conn_params` 4 s after connect; earlier requests are overwritten by the proxy's own "medium
conn params" after service discovery. Read the proxy's `Conn params updated` line (VERBOSE, API logs) to confirm;
the peer side never logs the event. The proxy only connects to a device its scanner has seen, so `btdrive.py`
subscribes to raw advertisements first.

Measure the peer's own disconnect (`serial_capture.py --ms`) after the client's disconnect: about 0.2 to 0.8 s when
the terminate arrived, about 6 s (the supervision timeout) when it was lost. The client side disconnect time is the
API reply and says nothing. On dev the S3 lost 6 of 12; the PR lost 0 of 16.

`run.sh LABEL ROUNDS` writes `central_bda.hpp` from `PROXY_BT_MAC`, builds and flashes both boards, runs the rounds
and `an.py`, which prints PASS when every peer disconnect came within 3 s.
