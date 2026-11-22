from esphome.components.zephyr import zephyr_enable_poweroff
from esphome.core import CORE

CODEOWNERS = ["@esphome/core", "@jsuanet"]
DOMAIN = "shutdown"


def request_power_off() -> None:
    """Enable the Zephyr power off that shutdown_device() calls."""
    if CORE.using_zephyr:
        zephyr_enable_poweroff()
