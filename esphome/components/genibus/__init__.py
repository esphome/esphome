import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@tomwellnitz"]
DOMAIN = "genibus"

CONF_GENIBUS_ID = "genibus_id"
CONF_MASTER_ADDRESS = "master_address"

genibus_ns = cg.esphome_ns.namespace("genibus")
GenibusBus = genibus_ns.class_("GenibusBus", cg.Component)
GenibusDevice = genibus_ns.class_(
    "GenibusDevice", cg.PollingComponent, cg.Parented.template(GenibusBus)
)


def genibus_bus_schema(default_master_address: int) -> cv.Schema:
    """Base schema of a transport that derives from GenibusBus; the transport picks the default address."""
    return cv.Schema(
        {
            cv.Optional(
                CONF_MASTER_ADDRESS, default=default_master_address
            ): cv.hex_uint8_t,
        }
    ).extend(cv.COMPONENT_SCHEMA)


def genibus_device_schema(default_address: int) -> cv.Schema:
    """Base schema of a device on the bus; the device picks its default address.

    It holds no component schema: the device extends it with cv.polling_component_schema().
    """
    return cv.Schema(
        {
            cv.GenerateID(CONF_GENIBUS_ID): cv.use_id(GenibusBus),
            cv.Optional(CONF_ADDRESS, default=default_address): cv.hex_uint8_t,
        }
    )


_register_device = cg.slot_list("GENIBUS_DEVICE_COUNT", "register_device", DOMAIN)


def apply_genibus_bus_config(var: MockObj, config: ConfigType) -> None:
    """Apply the settings of genibus_bus_schema() to a transport; the transport registers itself as a component."""
    cg.add(var.set_master_address(config[CONF_MASTER_ADDRESS]))


async def register_genibus_device(var: MockObj, config: ConfigType) -> None:
    """Register a device created with genibus_device_schema(), and give it a request slot on its bus."""
    await cg.register_component(var, config)
    bus = await cg.get_variable(config[CONF_GENIBUS_ID])
    await cg.register_parented(var, bus)
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(_register_device(bus, var))
