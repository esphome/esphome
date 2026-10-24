import esphome.codegen as cg
from esphome.components.canbus import CONF_CANBUS_ID, CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["canbus"]
DOMAIN = "toptronic"
MULTI_CONF = True

CONF_DEVICE_ADDR = "device_addr"
CONF_DEVICE_TYPE = "device_type"

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)

DEVICE_TYPES = {
    "WEZ": 0,  # heat generator
    "SOL": 64,  # solar module
    "PS": 128,  # buffer storage tank
    "FW": 192,  # district heating
    "HK": 256,  # heating circuit
    "MWA": 384,  # energy meter module
    "GLT": 448,  # building management system
    "HV": 512,  # HomeVent ventilation
    "BM": 1024,  # control module (display)
    "BD": 1024,  # alias of BM
    "GW": 1153,  # Modbus/KNX gateway
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TopTronicComponent),
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Required(CONF_DEVICE_TYPE): cv.enum(DEVICE_TYPES, upper=True),
        # Device types are 64 apart, so a larger address would alias the next type
        cv.Required(CONF_DEVICE_ADDR): cv.int_range(min=0, max=63),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    cbus = await cg.get_variable(config[CONF_CANBUS_ID])
    device_id = DEVICE_TYPES[config[CONF_DEVICE_TYPE]] | config[CONF_DEVICE_ADDR]
    var = cg.new_Pvariable(config[CONF_ID], cbus, device_id)
    await cg.register_component(var, config)
