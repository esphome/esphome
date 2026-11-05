# hdmi_cec

## HDMI-CEC two-board wire (#9189)

Two boards on one wire: promiscuous mode, a 0x8F request answered with 0x90 0x00, a `destination:`
filter on the reply handler, template buttons for each target plus lambdas calling
`send(source, dest, span)` as another initiator. Healthy numbers: device-side turnaround 15 to 22 ms,
round trip 144 to 156 ms. Press buttons over the API; judge timings from device logs (ESP32 light sleep
lags the press).
