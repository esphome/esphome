from esphome.components.zephyr import zephyr_add_prj_conf

DEPENDENCIES = ["zephyr_ble_server"]
AUTO_LOAD = ["zephyr_mcumgr"]


async def to_code(config):
    zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT", True)
    zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT_REASSEMBLY", True)

    zephyr_add_prj_conf("MCUMGR_GRP_OS", True)
    zephyr_add_prj_conf("MCUMGR_GRP_OS_MCUMGR_PARAMS", True)

    zephyr_add_prj_conf("NCS_SAMPLE_MCUMGR_BT_OTA_DFU_SPEEDUP", True)
