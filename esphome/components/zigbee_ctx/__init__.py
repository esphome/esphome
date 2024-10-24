import esphome.codegen as cg
from esphome.core import CORE, coroutine_with_priority

KEY_ZIGBEE = "zigbee"
KEY_EP = "ep"


def zigbee_set_core_data(config):
    CORE.data[KEY_ZIGBEE] = {}
    CORE.data[KEY_ZIGBEE][KEY_EP] = []
    return config


@coroutine_with_priority(10.0)
async def to_code(config):
    if len(CORE.data[KEY_ZIGBEE][KEY_EP]) > 0:
        cg.add_global(
            cg.RawExpression(
                f"ZBOSS_DECLARE_DEVICE_CTX_EP_VA(zb_device_ctx, &{', &'.join(CORE.data[KEY_ZIGBEE][KEY_EP])})"
            )
        )
        cg.add(cg.RawExpression("ZB_AF_REGISTER_DEVICE_CTX(&zb_device_ctx)"))
