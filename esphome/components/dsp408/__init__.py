"""ESPHome component for the Dayton Audio DSP-408 over native USB host.

Talks to a USB-attached DSP-408 (VID 0x0483 / PID 0x5750) directly from
an ESP32-S3 / S2 / P4's native USB host peripheral. Protocol port of
dsp408-py (https://github.com/malaiwah/dsp408-py) — same 64-byte HID
frame layout, same command codes, same field semantics.
"""

import esphome.codegen as cg
from esphome.components.usb_host import register_usb_client, usb_device_schema

CODEOWNERS = ["@malaiwah"]
DEPENDENCIES = ["esp32", "usb_host"]
# AUTO_LOAD pulls in the platforms our C++ unconditionally references.
# Sub-platform Python files (number/, switch/, text_sensor/, select/, text/)
# are also auto-loaded by ESPHome when their entities appear in the user's
# YAML, but listing them here ensures the C++ includes always resolve even
# in a minimal config that only declares the parent.
AUTO_LOAD = ["usb_host", "text", "select"]
MULTI_CONF = True

# Used by sub-platform configs to attach entities to a specific DSP-408
# instance via ``dsp408_id:`` references.
CONF_DSP408_ID = "dsp408_id"
# Shared by every sub-platform to disambiguate the entity role
# (master vs per-channel volume, HPF type vs LPF slope, etc.). Defined
# once here to satisfy ci-custom (constants used in >=5 files must
# either live here or in esphome/components/const/__init__.py).
CONF_KIND = "kind"

dsp408_ns = cg.esphome_ns.namespace("dsp408")
DSP408 = dsp408_ns.class_("DSP408", cg.Component)

# Default to the DSP-408's known USB IDs but allow override (e.g. if
# someone wants to use this on the sibling DSP-816 firmware which
# shares the protocol skeleton).
CONFIG_SCHEMA = usb_device_schema(DSP408, vid=0x0483, pid=0x5750)


async def to_code(config):
    return await register_usb_client(config)
