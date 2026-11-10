from esphome.components.zephyr import ZephyrData
from esphome.components.zephyr.const import KEY_ZEPHYR_RTOS
from esphome.core import CORE

CODEOWNERS = ["@rwrozelle"]
DOMAIN = "zephyr_rtos"
AUTO_LOAD = ["zephyr"]


class ZephyrRtosData(ZephyrData):
    """CORE.data store of a zephyr_rtos build; nrf52 builds use ZephyrData."""


def zephyr_rtos_data() -> ZephyrRtosData:
    return CORE.data[KEY_ZEPHYR_RTOS]
