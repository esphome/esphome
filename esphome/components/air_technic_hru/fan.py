import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    modbus,
    sensor,
    number,
    switch,
    binary_sensor,
    button,
    select,
    fan,
)
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_CO2,
    CONF_SPEED_COUNT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
    UNIT_MINUTE,
    UNIT_HOUR,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_SAFETY,
    STATE_CLASS_MEASUREMENT,
    ICON_THERMOMETER,
    ICON_FAN,
    ICON_TIMER,
    ICON_MOLECULE_CO2,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

AUTO_LOAD = [
    "sensor",
    "modbus",
    "switch",
    "number",
    "binary_sensor",
    "button",
    "select",
    "fan",
]

CONF_SUPPLY_TEMPERATURE = "supply_temperature"
CONF_RETURN_TEMPERATURE = "return_temperature"
CONF_EXTERNAL_TEMPERATURE = "external_temperature"
CONF_DEFROST_TEMPERATURE = "defrost_temperature"
CONF_FILTER_HOUR_COUNT = "filter_hour_count"
CONF_EXHAUST_FAN_SPEED = "exhaust_fan_speed"
CONF_SUPPLY_FAN_SPEED = "supply_fan_speed"

CONF_FIRE_ALARM = "fire_alarm"
CONF_BYPASS_OPEN = "bypass_open"
CONF_DEFROSTING = "defrosting"

CONF_HEATER_INSTALLED = "heater_installed"
CONF_START_AFTER_POWER_LOSS = "start_after_power_loss"
CONF_EXTERNAL_CONTROL = "external_control"
CONF_CO2_SENSOR_INSTALLED = "co2_sensor_installed"

CONF_SUPPLY_EXHAUST_RATIO = "supply_exhaust_ratio"
CONF_BYPASS_OPEN_TEMPERATURE = "bypass_open_temperature"
CONF_BYPASS_TEMPERATURE_RANGE = "bypass_temperature_range"

CONF_DEFROST_INTERVAL_TIME = "defrost_interval_time"
CONF_DEFROST_START_TEMPERATURE = "defrost_start_temperature"
CONF_DEFROST_DURATION = "defrost_duration"
CONF_FILTER_ALARM_INTERVAL = "filter_alarm_interval"

CONF_RESET_FILTER_HOURS = "reset_filter_hours"

air_technic_hru_ns = cg.esphome_ns.namespace("air_technic_hru")
AirTechnicHru = air_technic_hru_ns.class_(
    "AirTechnicHru", fan.Fan, cg.PollingComponent, modbus.ModbusDevice
)
AirTechnicHRUSwitch = air_technic_hru_ns.class_(
    "AirTechnicHRUSwitch", switch.Switch, cg.Component
)
AirTechnicHRUNumber = air_technic_hru_ns.class_(
    "AirTechnicHRUNumber", number.Number, cg.Component
)
AirTechnicHRUButton = air_technic_hru_ns.class_(
    "AirTechnicHRUButton", button.Button, cg.Component
)
AirTechnicHRUSelect = air_technic_hru_ns.class_(
    "AirTechnicHRUSelect", select.Select, cg.Component
)

CONFIG_SCHEMA = (
    fan.FAN_SCHEMA.extend(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(AirTechnicHru),
            cv.Optional(CONF_SPEED_COUNT, default=14): cv.int_range(min=1),
            # ------------ SENSORS --------------
            cv.Optional(
                CONF_SUPPLY_TEMPERATURE,
                default={CONF_NAME: "Supply Air Temperature"},
            ): sensor.sensor_schema(
                accuracy_decimals=0,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_THERMOMETER,
            ),
            cv.Optional(
                CONF_EXTERNAL_TEMPERATURE, default={CONF_NAME: "External Air Temperature"}
            ): sensor.sensor_schema(
                accuracy_decimals=0,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_THERMOMETER,
            ),
            cv.Optional(
                CONF_RETURN_TEMPERATURE, default={CONF_NAME: "Return Air Temperature"}
            ): sensor.sensor_schema(
                accuracy_decimals=0,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_THERMOMETER,
            ),
            cv.Optional(
                CONF_CO2,
            ): sensor.sensor_schema(
                accuracy_decimals=0,
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_MOLECULE_CO2,
            ),
            cv.Optional(
                CONF_DEFROST_TEMPERATURE,
            ): sensor.sensor_schema(
                accuracy_decimals=0,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_THERMOMETER,
            ),
            cv.Optional(
                CONF_FILTER_HOUR_COUNT,
                default={CONF_NAME: "Filter Hour Count"},
            ): sensor.sensor_schema(
                accuracy_decimals=1,
                unit_of_measurement=UNIT_HOUR,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_TIMER,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(
                CONF_SUPPLY_FAN_SPEED, default={CONF_NAME: "Supply Fan Speed"}
            ): sensor.sensor_schema(
                device_class=DEVICE_CLASS_SPEED,
                icon=ICON_FAN,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                accuracy_decimals=0,
            ).extend(
                cv.COMPONENT_SCHEMA
            ),
            cv.Optional(
                CONF_EXHAUST_FAN_SPEED, default={CONF_NAME: "Exhaust Fan Speed"}
            ): sensor.sensor_schema(
                device_class=DEVICE_CLASS_SPEED,
                icon=ICON_FAN,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                accuracy_decimals=0,
            ).extend(
                cv.COMPONENT_SCHEMA
            ),
            # -------- BINARY SENSORS ----------
            cv.Optional(
                CONF_BYPASS_OPEN, default={CONF_NAME: "Bypass"}
            ): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OPENING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(
                CONF_FIRE_ALARM, default={CONF_NAME: "Fire Alarm"}
            ): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_SAFETY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_DEFROSTING): binary_sensor.binary_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            # ----------- SWITCHES -------------
            cv.Optional(CONF_HEATER_INSTALLED): switch.switch_schema(
                AirTechnicHRUSwitch, entity_category=ENTITY_CATEGORY_CONFIG
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_START_AFTER_POWER_LOSS): switch.switch_schema(
                AirTechnicHRUSwitch, entity_category=ENTITY_CATEGORY_CONFIG
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_CO2_SENSOR_INSTALLED): switch.switch_schema(
                AirTechnicHRUSwitch, entity_category=ENTITY_CATEGORY_CONFIG
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_EXTERNAL_CONTROL): switch.switch_schema(
                AirTechnicHRUSwitch, entity_category=ENTITY_CATEGORY_CONFIG
            ).extend(cv.COMPONENT_SCHEMA),
            # ------------ NUMBERS --------------
            cv.Optional(
                CONF_SUPPLY_EXHAUST_RATIO,
                default={CONF_NAME: "Supply / Exhaust Ratio"},
            ): number.number_schema(
                AirTechnicHRUNumber,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ).extend(
                cv.COMPONENT_SCHEMA
            ),
            cv.Optional(
                CONF_BYPASS_OPEN_TEMPERATURE,
                default={CONF_NAME: "Bypass Open Temperature"},
            ): number.number_schema(
                AirTechnicHRUNumber,
                device_class=DEVICE_CLASS_TEMPERATURE,
                icon=ICON_THERMOMETER,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ).extend(
                cv.COMPONENT_SCHEMA
            ),
            cv.Optional(
                CONF_BYPASS_TEMPERATURE_RANGE,
                default={CONF_NAME: "Bypass Temperature Range"},
            ): number.number_schema(
                AirTechnicHRUNumber,
                device_class=DEVICE_CLASS_TEMPERATURE,
                icon=ICON_THERMOMETER,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ).extend(
                cv.COMPONENT_SCHEMA
            ),
            cv.Optional(CONF_DEFROST_INTERVAL_TIME): number.number_schema(
                AirTechnicHRUNumber,
                device_class=DEVICE_CLASS_DURATION,
                entity_category=ENTITY_CATEGORY_CONFIG,
                unit_of_measurement=UNIT_MINUTE,
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_DEFROST_DURATION): number.number_schema(
                AirTechnicHRUNumber,
                device_class=DEVICE_CLASS_DURATION,
                entity_category=ENTITY_CATEGORY_CONFIG,
                unit_of_measurement=UNIT_MINUTE,
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_DEFROST_START_TEMPERATURE): number.number_schema(
                AirTechnicHRUNumber,
                device_class=DEVICE_CLASS_TEMPERATURE,
                entity_category=ENTITY_CATEGORY_CONFIG,
                unit_of_measurement=UNIT_CELSIUS,
            ).extend(cv.COMPONENT_SCHEMA),
            # ------------ BUTTONS --------------
            cv.Optional(
                CONF_RESET_FILTER_HOURS, default={CONF_NAME: "Reset Filter Hours"}
            ): button.button_schema(
                AirTechnicHRUButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ).extend(
                cv.COMPONENT_SCHEMA
            ),
            # ------------ SELECTS --------------
            cv.Optional(
                CONF_FILTER_ALARM_INTERVAL, default={CONF_NAME: "Filter Alarm Interval"}
            ): select.select_schema(
                AirTechnicHRUSelect, entity_category=ENTITY_CATEGORY_CONFIG
            ).extend(
                cv.COMPONENT_SCHEMA
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbus.modbus_device_schema(0x01))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_SPEED_COUNT])
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)
    await fan.register_fan(var, config)

    # ------------ SENSORS --------------
    if CONF_SUPPLY_TEMPERATURE in config:
        conf = config[CONF_SUPPLY_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_supply_air_temperature_sensor(sens))

    if CONF_EXTERNAL_TEMPERATURE in config:
        conf = config[CONF_EXTERNAL_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_outdoor_temperature_sensor(sens))

    if CONF_RETURN_TEMPERATURE in config:
        conf = config[CONF_RETURN_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_room_temperature_sensor(sens))

    if CONF_CO2 in config:
        conf = config[CONF_CO2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_co2_sensor(sens))

    if CONF_DEFROST_TEMPERATURE in config:
        conf = config[CONF_DEFROST_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_defrost_temperature_sensor(sens))

    if CONF_FILTER_HOUR_COUNT in config:
        conf = config[CONF_FILTER_HOUR_COUNT]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_filter_hour_count_sensor(sens))

    if CONF_SUPPLY_FAN_SPEED in config:
        conf = config[CONF_SUPPLY_FAN_SPEED]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_supply_fan_speed_sensor(sens))

    if CONF_EXHAUST_FAN_SPEED in config:
        conf = config[CONF_EXHAUST_FAN_SPEED]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_exhaust_fan_speed_sensor(sens))

    # -------- BINARY SENSORS ----------
    if CONF_BYPASS_OPEN in config:
        conf = config[CONF_BYPASS_OPEN]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_bypass_open_sensor(sens))

    if CONF_FIRE_ALARM in config:
        conf = config[CONF_FIRE_ALARM]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_fire_alarm_sensor(sens))

    if CONF_DEFROSTING in config:
        conf = config[CONF_DEFROSTING]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_defrosting_sensor(sens))

    # ----------- SWITCHES -------------
    if CONF_HEATER_INSTALLED in config:
        conf = config[CONF_HEATER_INSTALLED]
        sw = await switch.new_switch(conf)
        await cg.register_component(sw, conf)
        cg.add(sw.set_parent(var))
        cg.add(sw.set_id(CONF_HEATER_INSTALLED))
        cg.add(var.set_heater_installed_switch(sw))

    if CONF_CO2_SENSOR_INSTALLED in config:
        conf = config[CONF_CO2_SENSOR_INSTALLED]
        sw = await switch.new_switch(conf)
        await cg.register_component(sw, conf)
        cg.add(sw.set_parent(var))
        cg.add(sw.set_id(CONF_CO2_SENSOR_INSTALLED))
        cg.add(var.set_co2_sensor_installed_switch(sw))

    if CONF_EXTERNAL_CONTROL in config:
        conf = config[CONF_EXTERNAL_CONTROL]
        sw = await switch.new_switch(conf)
        await cg.register_component(sw, conf)
        cg.add(sw.set_parent(var))
        cg.add(sw.set_id(CONF_EXTERNAL_CONTROL))
        cg.add(var.set_external_control_switch(sw))

    if CONF_START_AFTER_POWER_LOSS in config:
        conf = config[CONF_START_AFTER_POWER_LOSS]
        sw = await switch.new_switch(conf)
        await cg.register_component(sw, conf)
        cg.add(sw.set_parent(var))
        cg.add(sw.set_id(CONF_START_AFTER_POWER_LOSS))
        cg.add(var.set_power_restore_switch(sw))

    # ------------ NUMBERS --------------
    if CONF_SUPPLY_EXHAUST_RATIO in config:
        conf = config[CONF_SUPPLY_EXHAUST_RATIO]
        num = await number.new_number(conf, min_value=-1, max_value=1, step=0.1)
        await cg.register_component(num, conf)
        cg.add(num.set_parent(var))
        cg.add(num.set_id(CONF_SUPPLY_EXHAUST_RATIO))
        cg.add(var.set_supply_exhaust_ratio_number(num))

    if CONF_BYPASS_OPEN_TEMPERATURE in config:
        conf = config[CONF_BYPASS_OPEN_TEMPERATURE]
        num = await number.new_number(conf, min_value=5, max_value=30, step=1)
        await cg.register_component(num, conf)
        cg.add(num.set_parent(var))
        cg.add(num.set_id(CONF_BYPASS_OPEN_TEMPERATURE))
        cg.add(var.set_bypass_open_temperature_number(num))

    if CONF_BYPASS_TEMPERATURE_RANGE in config:
        conf = config[CONF_BYPASS_TEMPERATURE_RANGE]
        num = await number.new_number(conf, min_value=2, max_value=15, step=1)
        await cg.register_component(num, conf)
        cg.add(num.set_parent(var))
        cg.add(num.set_id(CONF_BYPASS_TEMPERATURE_RANGE))
        cg.add(var.set_bypass_temperature_range_number(num))

    if CONF_DEFROST_INTERVAL_TIME in config:
        conf = config[CONF_DEFROST_INTERVAL_TIME]
        num = await number.new_number(conf, min_value=15, max_value=99, step=1)
        await cg.register_component(num, conf)
        cg.add(num.set_parent(var))
        cg.add(num.set_id(CONF_DEFROST_INTERVAL_TIME))
        cg.add(var.set_defrost_interval_time_number(num))

    if CONF_DEFROST_DURATION in config:
        conf = config[CONF_DEFROST_DURATION]
        num = await number.new_number(conf, min_value=2, max_value=20, step=1)
        await cg.register_component(num, conf)
        cg.add(num.set_parent(var))
        cg.add(num.set_id(CONF_DEFROST_DURATION))
        cg.add(var.set_defrost_duration_number(num))

    if CONF_DEFROST_START_TEMPERATURE in config:
        conf = config[CONF_DEFROST_START_TEMPERATURE]
        num = await number.new_number(conf, min_value=-9, max_value=5, step=1)
        await cg.register_component(num, conf)
        cg.add(num.set_parent(var))
        cg.add(num.set_id(CONF_DEFROST_START_TEMPERATURE))
        cg.add(var.set_defrost_start_temperature_number(num))

    # ------------ BUTTONS --------------
    if CONF_RESET_FILTER_HOURS in config:
        conf = config[CONF_RESET_FILTER_HOURS]
        but = await button.new_button(conf)
        await cg.register_component(but, conf)
        cg.add(but.set_parent(var))
        cg.add(but.set_id(CONF_RESET_FILTER_HOURS))

    # ------------ SELECTS --------------
    if CONF_FILTER_ALARM_INTERVAL in config:
        conf = config[CONF_FILTER_ALARM_INTERVAL]
        sel = await select.new_select(
            conf, options=["45 days", "60 days", "90 days", "180 days"]
        )
        await cg.register_component(sel, conf)
        cg.add(sel.set_parent(var))
        cg.add(sel.set_id(CONF_FILTER_ALARM_INTERVAL))
        cg.add(var.set_filter_alarm_interval_select(sel))
