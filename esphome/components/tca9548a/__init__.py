import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_CHANNELS,
    CONF_FREQUENCY,
    CONF_I2C_ID,
    CONF_ID,
)
from esphome.core import CORE, ID
from esphome.types import ConfigType

CODEOWNERS = ["@andreashergert1984"]
DOMAIN = "tca9548a"

DEPENDENCIES = ["i2c"]

tca9548a_ns = cg.esphome_ns.namespace("tca9548a")
TCA9548AComponent = tca9548a_ns.class_("TCA9548AComponent", cg.Component, i2c.I2CDevice)
TCA9548AChannel = tca9548a_ns.class_("TCA9548AChannel", i2c.I2CBus)

MULTI_CONF = True

CONF_BUS_ID = "bus_id"
# Sizes the root bus's frequency table; only emitted when a frequency is set
_request_port_frequency_slot = cg.slot_counter("I2C_PORT_FREQUENCY_COUNT")


def _root_bus_id(bus_id: ID) -> ID:
    """The hardware bus behind a port, through any multiplexers in between."""
    parents = {
        str(chan[CONF_BUS_ID]): mux[CONF_I2C_ID]
        for mux in CORE.config.get(DOMAIN, [])
        for chan in mux.get(CONF_CHANNELS, [])
    }
    while (parent := parents.get(str(bus_id))) is not None:
        bus_id = parent
    return bus_id


def _declared_bus_type(bus_id: ID) -> cg.MockObjClass | None:
    """The class the i2c component declared for a bus, or None for an external bus."""
    for bus in CORE.config.get(i2c.DOMAIN, []):
        if bus[CONF_ID].id == bus_id.id:
            return bus[CONF_ID].type
    return None


async def _add_frequency(root_bus_id: ID, frequency: int) -> None:
    """Count the frequency against the root bus; the built-in ESP32 bus also
    creates its device handle in setup()."""
    _request_port_frequency_slot(str(root_bus_id))
    if _declared_bus_type(root_bus_id) is i2c.IDFI2CBus:
        root = await cg.get_variable(root_bus_id)
        cg.add(root.add_frequency(frequency))


_FREQUENCY = cv.All(cv.frequency, cv.Range(min=0, min_included=False))

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TCA9548AComponent),
            cv.Optional(CONF_FREQUENCY): _FREQUENCY,
            cv.Optional(CONF_CHANNELS, default=[]): cv.ensure_list(
                {
                    cv.Required(CONF_BUS_ID): cv.declare_id(TCA9548AChannel),
                    cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
                    cv.Optional(CONF_FREQUENCY): _FREQUENCY,
                }
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x70))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    root_bus_id = _root_bus_id(config[CONF_I2C_ID])
    if (frequency := config.get(CONF_FREQUENCY)) is not None:
        await _add_frequency(root_bus_id, int(frequency))
        cg.add(var.set_frequency(int(frequency)))

    for conf in config[CONF_CHANNELS]:
        chan = cg.new_Pvariable(conf[CONF_BUS_ID])
        cg.add(chan.set_parent(var))
        cg.add(chan.set_channel(conf[CONF_CHANNEL]))
        if (frequency := conf.get(CONF_FREQUENCY)) is not None:
            await _add_frequency(root_bus_id, int(frequency))
            cg.add(chan.set_frequency(int(frequency)))
