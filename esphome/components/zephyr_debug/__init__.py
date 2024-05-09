import esphome.config_validation as cv
from esphome.components.zephyr import zephyr_add_prj_conf

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    # gdb thread support
    zephyr_add_prj_conf("DEBUG_THREAD_INFO", True)
    # RTT
    zephyr_add_prj_conf("USE_SEGGER_RTT", True)
    zephyr_add_prj_conf("RTT_CONSOLE", True)
    zephyr_add_prj_conf("LOG", True)
    zephyr_add_prj_conf("LOG_BLOCK_IN_THREAD", True)
    zephyr_add_prj_conf("LOG_BUFFER_SIZE", 4096)
    zephyr_add_prj_conf("SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL", True)
