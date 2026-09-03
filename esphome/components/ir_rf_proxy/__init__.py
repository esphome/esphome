"""IR/RF Proxy component - provides remote_base backend for infrared platform."""

import esphome.codegen as cg

CODEOWNERS = ["@kbx81"]

# Namespace and constants exported for infrared.py platform
ir_rf_proxy_ns = cg.esphome_ns.namespace("ir_rf_proxy")

CONF_REMOTE_RECEIVER_ID = "remote_receiver_id"
CONF_REMOTE_TRANSMITTER_ID = "remote_transmitter_id"
