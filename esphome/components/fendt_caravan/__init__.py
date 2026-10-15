import esphome.codegen as cg
from esphome.components import binary_sensor, ble_client, sensor, switch, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor", "sensor", "switch", "text_sensor"]
CODEOWNERS = ["@rawsludge"]
DEPENDENCIES = ["ble_client"]
DOMAIN = "fendt_caravan"
MULTI_CONF = True

fendt_caravan_ns = cg.esphome_ns.namespace("fendt_caravan")
FendtCaravan = fendt_caravan_ns.class_("FendtCaravan", ble_client.BLEClientNode)
MainControlUnitHub = fendt_caravan_ns.class_("MainControlUnitHub", cg.Component)
FendtSwitch = fendt_caravan_ns.class_(
    "FendtSwitch", switch.Switch, cg.Parented.template(MainControlUnitHub)
)

CONF_BT_ID = "bt_id"
CONF_MAIN_CONTROL_UNIT = "main_control_unit"
CONF_TEMP_IN = "temp_in"
CONF_TEMP_OUT = "temp_out"
CONF_WATER_LEVEL = "water_level"
CONF_POWER_STATUS = "power_status"
CONF_SOFTWARE_VERSION = "software_version"
CONF_MAIN_SWITCH = "main_switch"
CONF_ALL_LIGHTS = "all_lights"
CONF_FLOOR_HEATER = "floor_heater"

# The address the panel was paired with; it drops the link when given any other one
DEFAULT_BT_ID = "C0:EE:FB:90:B0:A7"

_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

MAIN_CONTROL_UNIT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MainControlUnitHub),
        cv.Optional(CONF_TEMP_IN): _TEMPERATURE_SCHEMA,
        cv.Optional(CONF_TEMP_OUT): _TEMPERATURE_SCHEMA,
        cv.Optional(CONF_WATER_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            icon="mdi:water-percent",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER_STATUS): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_POWER, icon="mdi:power-plug"
        ),
        cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:application-braces-outline",
        ),
        cv.Optional(CONF_MAIN_SWITCH): switch.switch_schema(
            FendtSwitch, icon="mdi:power", default_restore_mode="DISABLED"
        ),
        cv.Optional(CONF_ALL_LIGHTS): switch.switch_schema(
            FendtSwitch, icon="mdi:lightbulb-group", default_restore_mode="DISABLED"
        ),
        cv.Optional(CONF_FLOOR_HEATER): switch.switch_schema(
            FendtSwitch, icon="mdi:heat-wave", default_restore_mode="DISABLED"
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FendtCaravan),
        cv.Optional(CONF_BT_ID, default=DEFAULT_BT_ID): cv.mac_address,
        cv.Required(CONF_MAIN_CONTROL_UNIT): MAIN_CONTROL_UNIT_SCHEMA,
    }
).extend(ble_client.BLE_CLIENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    # The handshake carries the paired address in lower case
    bt_id = ":".join(f"{part:02x}" for part in config[CONF_BT_ID].parts)
    var = cg.new_Pvariable(config[CONF_ID], f"net-BT_ID-{bt_id}")
    await ble_client.register_ble_node(var, config)

    mcu = config[CONF_MAIN_CONTROL_UNIT]
    hub = cg.new_Pvariable(mcu[CONF_ID], var)
    await cg.register_component(hub, mcu)
    sensors = sensor.sub_sensors(mcu)
    await sensors(CONF_TEMP_IN, hub.set_temp_in_sensor)
    await sensors(CONF_TEMP_OUT, hub.set_temp_out_sensor)
    await sensors(CONF_WATER_LEVEL, hub.set_water_level_sensor)
    binary_sensors = binary_sensor.sub_binary_sensors(mcu)
    await binary_sensors(CONF_POWER_STATUS, hub.set_power_status_binary_sensor)
    text_sensors = text_sensor.sub_text_sensors(mcu)
    await text_sensors(CONF_SOFTWARE_VERSION, hub.set_software_version_text_sensor)
    switches = switch.sub_switches(mcu, parent=hub)
    await switches(CONF_MAIN_SWITCH, hub.set_main_switch)
    await switches(CONF_ALL_LIGHTS, hub.set_all_lights_switch)
    await switches(CONF_FLOOR_HEATER, hub.set_floor_heater_switch)
