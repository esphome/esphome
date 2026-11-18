"""Pioneer WT series mini splits, controlled through the serial port of the WiFi module."""

import esphome.codegen as cg

CODEOWNERS = ["@martinsah"]
DOMAIN = "pioneer_wt"

pioneer_wt_ns = cg.esphome_ns.namespace("pioneer_wt")

CONF_PIONEER_WT_ID = "pioneer_wt_id"
