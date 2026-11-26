#!/bin/bash
# atlines.sh CAPTURE: the modem and AT lines of a serial capture, including the raw reply lines esp_modem prints
# after its "Response:" lines (no [..] prefix), with the Wi-Fi driver noise removed.
LC_ALL=C tr -d '\r' < "${1:?capture}" |
  LC_ALL=C grep -a -E "command_lib|\[modem|modem (dis)?connected|rst:|Guru|abort|Backtrace|traffic|^\[[0-9:]+\][^][ ]" |
  LC_ALL=C grep -av -E "^\[[0-9:]+\][IDWE] \(|wifi:000|^\[[0-9:]+\]$"
