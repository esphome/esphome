import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@randybb"]

DOMAIN = "geolocation"
IS_PLATFORM_COMPONENT = True

CONF_FIELD_AGE = "field_age"
CONF_GEOID_SEPARATION = "geoid_separation"
CONF_GEOLOCATION_ID = "geolocation_id"
CONF_MILLISECOND = "millisecond"

geolocation_ns = cg.esphome_ns.namespace("geolocation")
GeolocationComponent = geolocation_ns.class_(
    "GeolocationComponent", cg.PollingComponent
)
GeolocationData = geolocation_ns.class_("GeolocationData")

# Sensors publish the latest data on each update_interval
DATA_PUBLISHER = cg.HubFunction(
    "data",
    cg.void,
    [(GeolocationData.const_ref(), "data")],
    define="USE_GEOLOCATION_DATA_PUBLISHER",
)
# Consumers that need every new solution as it arrives (time)
FIX_PUBLISHER = cg.HubFunction(
    "fix",
    cg.void,
    [(GeolocationData.const_ref(), "data")],
    define="USE_GEOLOCATION_FIX_PUBLISHER",
)
DATA = DATA_PUBLISHER.param("data")
FIX = FIX_PUBLISHER.param("data")


def request_field_age() -> None:
    """Compile in GeolocationComponent::get_age(), which costs 4 bytes of RAM per field."""
    cg.add_define("USE_GEOLOCATION_FIELD_AGE")


def request_millisecond() -> None:
    """Compile in GeolocationData::millisecond."""
    cg.add_define("USE_GEOLOCATION_MILLISECOND")


def request_geoid_separation() -> None:
    """Compile in GeolocationData::geoid_separation_e3."""
    cg.add_define("USE_GEOLOCATION_GEOID_SEPARATION")


GEOLOCATION_CHILD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GEOLOCATION_ID): cv.use_id(GeolocationComponent),
    }
)


def geolocation_schema(class_: MockObjClass) -> cv.Schema:
    """The schema of a geolocation platform."""
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            # For lambdas; components that need these request them
            cv.Optional(CONF_FIELD_AGE, default=False): cv.boolean,
            cv.Optional(CONF_MILLISECOND, default=False): cv.boolean,
            cv.Optional(CONF_GEOID_SEPARATION, default=False): cv.boolean,
        }
    ).extend(cv.polling_component_schema("20s"))


async def new_geolocation(config: ConfigType) -> MockObj:
    """Create and register a geolocation platform."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if config[CONF_FIELD_AGE]:
        request_field_age()
    if config[CONF_MILLISECOND]:
        request_millisecond()
    if config[CONF_GEOID_SEPARATION]:
        request_geoid_separation()
    return var
