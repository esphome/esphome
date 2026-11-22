from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.core import CORE

CODEOWNERS = ["@esphome/core", "@jsuanet"]
DOMAIN = "shutdown"


def request_power_off() -> None:
    """Enable the Zephyr power off that the shutdown action calls."""
    # Zigbee builds leave it out, as deep_sleep does
    if CORE.using_zephyr and "zigbee" not in CORE.loaded_integrations:
        zephyr_add_prj_conf("POWEROFF", True)
