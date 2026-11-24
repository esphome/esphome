#!/bin/bash
# hostlog.sh OUT SECONDS [MATCH]: macOS `log stream` (no sudo needed) of the USB and NCM kernel, DriverKit and configd
# lines for SECONDS. MATCH (default W5100S) is a word from the device's USB product string. Read it with hostorder.py.
M=${3:-W5100S}
exec timeout "$2" log stream --style compact --level debug --predicate "(process == \"kernel\" AND (eventMessage CONTAINS[c] \"usb\" OR eventMessage CONTAINS[c] \"ncm\" OR eventMessage CONTAINS[c] \"ecm\" OR eventMessage CONTAINS[c] \"$M\")) OR process BEGINSWITH \"com.apple.DriverKit\" OR process CONTAINS \"USB\" OR ((process == \"configd\" OR process == \"IPConfiguration\") AND eventMessage CONTAINS[c] \"ncm\")" > "$1" 2>&1
