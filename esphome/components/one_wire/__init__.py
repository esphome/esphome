import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_INDEX

CODEOWNERS = ["@ssieb"]

IS_PLATFORM_COMPONENT = True

CONF_ONE_WIRE_ID = "one_wire_id"

one_wire_ns = cg.esphome_ns.namespace("one_wire")
OneWireBus = one_wire_ns.class_("OneWireBus")
OneWireDevice = one_wire_ns.class_("OneWireDevice")


def one_wire_device_schema():
    """Create a schema for a 1-wire device.

    :return: The 1-wire device schema, `extend` this in your config schema.
    """
    return cv.Schema(
        {
            cv.GenerateID(CONF_ONE_WIRE_ID): cv.use_id(OneWireBus),
            cv.Exclusive(CONF_ADDRESS, "index_or_address"): cv.hex_uint64_t,
            cv.Exclusive(CONF_INDEX, "index_or_address"): cv.uint8_t,
        }
    )


async def register_one_wire_device(var, config):
    """Register an 1-wire device with the given config.

    Sets the 1-wire bus to use and the 1-wire address.

    This is a coroutine, you need to await it with a 'yield' expression!
    """
    parent = await cg.get_variable(config[CONF_ONE_WIRE_ID])
    cg.add(var.set_one_wire_bus(parent))
    if (address := config.get(CONF_ADDRESS)) is not None:
        cg.add(var.set_address(address))
    if (index := config.get(CONF_INDEX)) is not None:
        cg.add(var.set_index(index))
