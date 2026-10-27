# nrf52

## nRF52840 dongle boards (#19009, #20814)

Test config: the two LEDs as inverted GPIO outputs, the button as an inverted pull-up binary sensor, a
10 s alive interval. To retest the nrfutil download path, move the cached `sdk-nrf/nrfutil` aside first.

## ItsyBitsy over UF2 when serial DFU fails (#20481)

Serial DFU to the ItsyBitsy nRF52840 fails about half the time ("Target is not in DFU mode"). Reliable path: a
1200 baud touch on the app port, wait for the `ITSY840BOOT` volume, convert `zephyr/zephyr.hex` with the SDK's
`zephyr/scripts/build/uf2conv.py -f 0xADA52840 -c` and copy the .uf2 to the volume (the copy ends in an I/O error
when the board reboots, which is normal). macOS BLE scans give UUIDs, not MAC addresses.
