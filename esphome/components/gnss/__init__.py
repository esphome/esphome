import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType, SafeExpType

CODEOWNERS = ["@randybb"]

DOMAIN = "gnss"
IS_PLATFORM_COMPONENT = True

CONF_GNSS_ID = "gnss_id"

gnss_ns = cg.esphome_ns.namespace("gnss")
GNSSComponent = gnss_ns.class_("GNSSComponent", cg.PollingComponent)
GNSSData = gnss_ns.class_("GNSSData")

# Sensors publish the latest data on each update_interval
DATA_PUBLISHER = cg.HubFunction(
    "data",
    cg.void,
    [(GNSSData.const_ref(), "data")],
    define="USE_GNSS_DATA_PUBLISHER",
)
# Consumers that need every new solution as it arrives (time)
FIX_PUBLISHER = cg.HubFunction(
    "fix",
    cg.void,
    [(GNSSData.const_ref(), "data")],
    define="USE_GNSS_FIX_PUBLISHER",
)
DATA = DATA_PUBLISHER.param("data")
FIX = FIX_PUBLISHER.param("data")

GNSS_CHILD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GNSS_ID): cv.use_id(GNSSComponent),
    }
)


def gnss_schema(class_: MockObjClass) -> cv.Schema:
    """The schema of a gnss platform."""
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
        }
    ).extend(cv.polling_component_schema("20s"))


async def new_gnss(config: ConfigType, *args: SafeExpType) -> MockObj:
    """Create and register a gnss platform; extra args go to the constructor."""
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await cg.register_component(var, config)
    return var
