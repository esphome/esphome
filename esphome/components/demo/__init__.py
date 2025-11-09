import esphome.codegen as cg
from esphome.components import (
    alarm_control_panel,
    binary_sensor,
    button,
    climate,
    cover,
    datetime,
    event,
    fan,
    light,
    lock,
    number,
    select,
    sensor,
    switch,
    text,
    text_sensor,
    valve,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_BINARY_SENSORS,
    CONF_DEVICE_CLASS,
    CONF_FORCE_UPDATE,
    CONF_ICON,
    CONF_INVERTED,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_NAME,
    CONF_OPTIONS,
    CONF_OUTPUT_ID,
    CONF_SENSORS,
    CONF_STATE_CLASS,
    CONF_STEP,
    CONF_SWITCHES,
    CONF_TEXT_SENSORS,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_IDENTIFY,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_UPDATE,
    ICON_BLUETOOTH,
    ICON_BLUR,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_WATT_HOURS,
)

AUTO_LOAD = [
    "alarm_control_panel",
    "binary_sensor",
    "button",
    "climate",
    "cover",
    "datetime",
    "event",
    "fan",
    "light",
    "lock",
    "number",
    "select",
    "sensor",
    "switch",
    "text",
    "text_sensor",
    "valve",
]

demo_ns = cg.esphome_ns.namespace("demo")
DemoAlarmControlPanel = demo_ns.class_(
    "DemoAlarmControlPanel", alarm_control_panel.AlarmControlPanel, cg.Component
)
DemoAlarmControlPanelType = demo_ns.enum("DemoAlarmControlPanelType", is_class=True)
DemoBinarySensor = demo_ns.class_(
    "DemoBinarySensor", binary_sensor.BinarySensor, cg.PollingComponent
)
DemoButton = demo_ns.class_("DemoButton", button.Button)
DemoClimate = demo_ns.class_("DemoClimate", climate.Climate, cg.Component)
DemoClimateType = demo_ns.enum("DemoClimateType", is_class=True)
DemoCover = demo_ns.class_("DemoCover", cover.Cover, cg.Component)
DemoCoverType = demo_ns.enum("DemoCoverType", is_class=True)
DemoDate = demo_ns.class_("DemoDate", datetime.DateEntity, cg.Component)
DemoDateTime = demo_ns.class_("DemoDateTime", datetime.DateTimeEntity, cg.Component)
DemoTime = demo_ns.class_("DemoTime", datetime.TimeEntity, cg.Component)
DemoEvent = demo_ns.class_("DemoEvent", event.Event, cg.Component)
DemoFan = demo_ns.class_("DemoFan", fan.Fan, cg.Component)
DemoFanType = demo_ns.enum("DemoFanType", is_class=True)
DemoLight = demo_ns.class_("DemoLight", light.LightOutput, cg.Component)
DemoLightType = demo_ns.enum("DemoLightType", is_class=True)
DemoLock = demo_ns.class_("DemoLock", lock.Lock, cg.Component)
DemoLockType = demo_ns.enum("DemoLockType", is_class=True)
DemoNumber = demo_ns.class_("DemoNumber", number.Number, cg.Component)
DemoNumberType = demo_ns.enum("DemoNumberType", is_class=True)
DemoSelect = demo_ns.class_("DemoSelect", select.Select, cg.Component)
DemoSelectType = demo_ns.enum("DemoSelectType", is_class=True)
DemoSensor = demo_ns.class_("DemoSensor", sensor.Sensor, cg.PollingComponent)
DemoSwitch = demo_ns.class_("DemoSwitch", switch.Switch, cg.Component)
DemoText = demo_ns.class_("DemoText", text.Text, cg.Component)
DemoTextType = demo_ns.enum("DemoTextType", is_class=True)
DemoTextSensor = demo_ns.class_(
    "DemoTextSensor", text_sensor.TextSensor, cg.PollingComponent
)
DemoValve = demo_ns.class_("DemoValve", valve.Valve)
DemoValveType = demo_ns.enum("DemoValveType", is_class=True)


ALARM_CONTROL_PANEL_TYPES = {
    1: DemoAlarmControlPanelType.TYPE_1,
    2: DemoAlarmControlPanelType.TYPE_2,
    3: DemoAlarmControlPanelType.TYPE_3,
}
CLIMATE_TYPES = {
    1: DemoClimateType.TYPE_1,
    2: DemoClimateType.TYPE_2,
    3: DemoClimateType.TYPE_3,
}
COVER_TYPES = {
    1: DemoCoverType.TYPE_1,
    2: DemoCoverType.TYPE_2,
    3: DemoCoverType.TYPE_3,
    4: DemoCoverType.TYPE_4,
}
FAN_TYPES = {
    1: DemoFanType.TYPE_1,
    2: DemoFanType.TYPE_2,
    3: DemoFanType.TYPE_3,
    4: DemoFanType.TYPE_4,
}
LIGHT_TYPES = {
    1: DemoLightType.TYPE_1,
    2: DemoLightType.TYPE_2,
    3: DemoLightType.TYPE_3,
    4: DemoLightType.TYPE_4,
    5: DemoLightType.TYPE_5,
    6: DemoLightType.TYPE_6,
    7: DemoLightType.TYPE_7,
}
LOCK_TYPES = {
    1: DemoLockType.TYPE_1,
    2: DemoLockType.TYPE_2,
}
NUMBER_TYPES = {
    1: DemoNumberType.TYPE_1,
    2: DemoNumberType.TYPE_2,
    3: DemoNumberType.TYPE_3,
}
TEXT_TYPES = {
    1: DemoTextType.TYPE_1,
    2: DemoTextType.TYPE_2,
}
VALVE_TYPES = {
    1: DemoValveType.TYPE_1,
    2: DemoValveType.TYPE_2,
}


CONF_ALARM_CONTROL_PANELS = "alarm_control_panels"
CONF_BUTTONS = "buttons"
CONF_CLIMATES = "climates"
CONF_COVERS = "covers"
CONF_DATETIMES = "datetimes"
CONF_FANS = "fans"
CONF_LIGHTS = "lights"
CONF_LOCKS = "locks"
CONF_NUMBERS = "numbers"
CONF_SELECTS = "selects"
CONF_TEXTS = "texts"
CONF_VALVES = "valves"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(
            CONF_ALARM_CONTROL_PANELS,
            default=[
                {
                    CONF_NAME: "Demo Alarm Control Panel",
                    CONF_TYPE: 1,
                },
                {
                    CONF_NAME: "Demo Alarm Control Panel Code",
                    CONF_TYPE: 2,
                },
                {
                    CONF_NAME: "Demo Alarm Control Panel Code to Arm",
                    CONF_TYPE: 3,
                },
            ],
        ): [
            alarm_control_panel.alarm_control_panel_schema(
                DemoAlarmControlPanel
            ).extend(
                {
                    cv.Required(CONF_TYPE): cv.enum(
                        ALARM_CONTROL_PANEL_TYPES, int=True
                    ),
                }
            )
        ],
        cv.Optional(
            CONF_BINARY_SENSORS,
            default=[
                {
                    CONF_NAME: "Demo Basement Floor Wet",
                    CONF_DEVICE_CLASS: DEVICE_CLASS_MOISTURE,
                },
                {
                    CONF_NAME: "Demo Movement Backyard",
                    CONF_DEVICE_CLASS: DEVICE_CLASS_MOTION,
                },
            ],
        ): [
            binary_sensor.binary_sensor_schema(DemoBinarySensor).extend(
                cv.polling_component_schema("60s")
            )
        ],
        cv.Optional(
            CONF_BUTTONS,
            default=[
                {
                    CONF_NAME: "Demo Update Button",
                    CONF_DEVICE_CLASS: DEVICE_CLASS_UPDATE,
                },
                {
                    CONF_NAME: "Demo Button Identify",
                    CONF_DEVICE_CLASS: DEVICE_CLASS_IDENTIFY,
                },
            ],
        ): [
            button.button_schema(DemoButton),
        ],
        cv.Optional(
            CONF_CLIMATES,
            default=[
                {
                    CONF_NAME: "Demo Heatpump",
                    CONF_TYPE: 1,
                },
                {
                    CONF_NAME: "Demo HVAC",
                    CONF_TYPE: 2,
                },
                {
                    CONF_NAME: "Demo Ecobee",
                    CONF_TYPE: 3,
                },
            ],
        ): [
            climate.climate_schema(DemoClimate)
            .extend(cv.COMPONENT_SCHEMA)
            .extend(
                {
                    cv.Required(CONF_TYPE): cv.enum(CLIMATE_TYPES, int=True),
                }
            )
        ],
        cv.Optional(
            CONF_COVERS,
            default=[
                {
                    CONF_NAME: "Demo Kitchen Window",
                    CONF_TYPE: 1,
                },
                {
                    CONF_NAME: "Demo Garage Door",
                    CONF_TYPE: 2,
                    CONF_DEVICE_CLASS: "garage",
                },
                {
                    CONF_NAME: "Demo Living Room Window",
                    CONF_TYPE: 3,
                },
                {
                    CONF_NAME: "Demo Hall Window",
                    CONF_TYPE: 4,
                    CONF_DEVICE_CLASS: "window",
                },
            ],
        ): [
            cover.cover_schema(DemoCover)
            .extend(cv.COMPONENT_SCHEMA)
            .extend(
                {
                    cv.Required(CONF_TYPE): cv.enum(COVER_TYPES, int=True),
                }
            )
        ],
        cv.Optional(
            CONF_DATETIMES,
            default=[
                {CONF_NAME: "Demo DateTime", CONF_TYPE: "DATETIME"},
                {CONF_NAME: "Demo Date", CONF_TYPE: "DATE"},
                {CONF_NAME: "Demo Time", CONF_TYPE: "TIME"},
            ],
        ): [
            cv.Any(
                datetime.date_schema(DemoDate),
                datetime.datetime_schema(DemoDateTime),
                datetime.time_schema(DemoTime),
            )
        ],
        cv.Optional(
            CONF_FANS,
            default=[
                {
                    CONF_NAME: "Demo Living Room Fan",
                    CONF_TYPE: 1,
                },
                {
                    CONF_NAME: "Demo Ceiling Fan",
                    CONF_TYPE: 2,
                },
                {
                    CONF_NAME: "Demo Percentage Limited Fan",
                    CONF_TYPE: 3,
                },
                {
                    CONF_NAME: "Demo Percentage Full Fan",
                    CONF_TYPE: 4,
                },
            ],
        ): [
            fan.fan_schema(DemoFan)
            .extend(cv.COMPONENT_SCHEMA)
            .extend(
                {
                    cv.Required(CONF_TYPE): cv.enum(FAN_TYPES, int=True),
                }
            )
        ],
        cv.Optional(
            CONF_LIGHTS,
            default=[
                {
                    CONF_NAME: "Demo Binary Light",
                    CONF_TYPE: 1,
                },
                {
                    CONF_NAME: "Demo Brightness Light",
                    CONF_TYPE: 2,
                },
                {
                    CONF_NAME: "Demo RGB Light",
                    CONF_TYPE: 3,
                },
                {
                    CONF_NAME: "Demo RGBW Light",
                    CONF_TYPE: 4,
                },
                {
                    CONF_NAME: "Demo RGBWW Light",
                    CONF_TYPE: 5,
                },
                {
                    CONF_NAME: "Demo CWWW Light",
                    CONF_TYPE: 6,
                },
                {
                    CONF_NAME: "Demo RGBW interlock Light",
                    CONF_TYPE: 7,
                },
            ],
        ): [
            light.light_schema(DemoLight, light.LightType.RGB)
            .extend(cv.COMPONENT_SCHEMA)
            .extend(
                {
                    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(DemoLight),
                    cv.Required(CONF_TYPE): cv.enum(LIGHT_TYPES, int=True),
                }
            )
        ],
        cv.Optional(
            CONF_LOCKS,
            default=[
                {CONF_NAME: "Demo Lock", CONF_TYPE: 1},
                {CONF_NAME: "Demo Lock and Open", CONF_TYPE: 2},
            ],
        ): [
            lock.lock_schema(DemoLock).extend(
                {
                    cv.Required(CONF_TYPE): cv.enum(LOCK_TYPES, int=True),
                }
            )
        ],
        cv.Optional(
            CONF_NUMBERS,
            default=[
                {
                    CONF_NAME: "Demo Number 0-100",
                    CONF_TYPE: 1,
                    CONF_MIN_VALUE: 0,
                    CONF_MAX_VALUE: 100,
                    CONF_STEP: 1,
                },
                {
                    CONF_NAME: "Demo Number -50-50",
                    CONF_TYPE: 2,
                    CONF_MIN_VALUE: -50,
                    CONF_MAX_VALUE: 50,
                    CONF_STEP: 5,
                },
                {
                    CONF_NAME: "Demo Number 40-60",
                    CONF_TYPE: 3,
                    CONF_MIN_VALUE: 40,
                    CONF_MAX_VALUE: 60,
                    CONF_STEP: 0.2,
                },
            ],
        ): [
            number.number_schema(DemoNumber)
            .extend(cv.COMPONENT_SCHEMA)
            .extend(
                {
                    cv.Required(CONF_TYPE): cv.enum(NUMBER_TYPES, int=True),
                    cv.Required(CONF_MIN_VALUE): cv.float_,
                    cv.Required(CONF_MAX_VALUE): cv.float_,
                    cv.Required(CONF_STEP): cv.float_,
                }
            )
        ],
        cv.Optional(
            CONF_SELECTS,
            default=[
                {
                    CONF_NAME: "Demo Select 1",
                    CONF_OPTIONS: ["Option 1", "Option 2", "Option 3"],
                },
                {
                    CONF_NAME: "Demo Select 2",
                    CONF_OPTIONS: ["Option A", "Option B", "Option C"],
                },
            ],
        ): [
            select.select_schema(DemoSelect).extend(
                {
                    cv.Required(CONF_OPTIONS): cv.ensure_list(cv.string_strict),
                }
            )
        ],
        cv.Optional(
            CONF_SENSORS,
            default=[
                {
                    CONF_NAME: "Demo Plain Sensor",
                },
                {
                    CONF_NAME: "Demo Temperature Sensor 1",
                    CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
                    CONF_ICON: ICON_THERMOMETER,
                    CONF_ACCURACY_DECIMALS: 1,
                    CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
                    CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
                },
                {
                    CONF_NAME: "Demo Temperature Sensor 2",
                    CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
                    CONF_ICON: ICON_THERMOMETER,
                    CONF_ACCURACY_DECIMALS: 1,
                    CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
                    CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
                },
                {
                    CONF_NAME: "Demo Force Update Sensor",
                    CONF_UNIT_OF_MEASUREMENT: UNIT_PERCENT,
                    CONF_ACCURACY_DECIMALS: 0,
                    CONF_DEVICE_CLASS: DEVICE_CLASS_HUMIDITY,
                    CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
                    CONF_FORCE_UPDATE: True,
                },
                {
                    CONF_NAME: "Demo Energy Sensor",
                    CONF_UNIT_OF_MEASUREMENT: UNIT_WATT_HOURS,
                    CONF_ACCURACY_DECIMALS: 0,
                    CONF_DEVICE_CLASS: DEVICE_CLASS_ENERGY,
                    CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
                },
            ],
        ): [
            sensor.sensor_schema(DemoSensor, accuracy_decimals=0).extend(
                cv.polling_component_schema("60s")
            )
        ],
        cv.Optional(
            CONF_SWITCHES,
            default=[
                {
                    CONF_NAME: "Demo Switch 1",
                },
                {
                    CONF_NAME: "Demo Switch 2",
                    CONF_INVERTED: True,
                    CONF_ICON: ICON_BLUETOOTH,
                },
            ],
        ): [switch.switch_schema(DemoSwitch).extend(cv.COMPONENT_SCHEMA)],
        cv.Optional(
            CONF_TEXTS,
            default=[
                {CONF_NAME: "Demo Text 1", CONF_MODE: "TEXT", CONF_TYPE: 1},
                {CONF_NAME: "Demo Text 2", CONF_MODE: "PASSWORD", CONF_TYPE: 2},
            ],
        ): [
            text.text_schema(DemoText).extend(
                {
                    cv.Required(CONF_TYPE): cv.enum(TEXT_TYPES, int=True),
                }
            )
        ],
        cv.Optional(
            CONF_TEXT_SENSORS,
            default=[
                {
                    CONF_NAME: "Demo Text Sensor 1",
                },
                {
                    CONF_NAME: "Demo Text Sensor 2",
                    CONF_ICON: ICON_BLUR,
                },
            ],
        ): [
            text_sensor.text_sensor_schema(DemoTextSensor).extend(
                cv.polling_component_schema("60s")
            )
        ],
        cv.Optional(
            CONF_VALVES,
            default=[
                {CONF_NAME: "Demo Valve 1", CONF_TYPE: 1},
                {CONF_NAME: "Demo Valve 2", CONF_TYPE: 2},
            ],
        ): [
            valve.valve_schema(DemoValve).extend(
                {
                    cv.Required(CONF_TYPE): cv.enum(VALVE_TYPES, int=True),
                }
            )
        ],
    }
)


async def to_code(config):
    for conf in config[CONF_ALARM_CONTROL_PANELS]:
        var = await alarm_control_panel.new_alarm_control_panel(conf)
        cg.add(var.set_type(conf[CONF_TYPE]))
        await cg.register_component(var, conf)

    for conf in config[CONF_BINARY_SENSORS]:
        var = await binary_sensor.new_binary_sensor(conf)
        await cg.register_component(var, conf)

    for conf in config[CONF_BUTTONS]:
        await button.new_button(conf)

    for conf in config[CONF_CLIMATES]:
        var = await climate.new_climate(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_type(conf[CONF_TYPE]))

    for conf in config[CONF_COVERS]:
        var = await cover.new_cover(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_type(conf[CONF_TYPE]))

    for conf in config[CONF_DATETIMES]:
        var = await datetime.new_datetime(conf)
        await cg.register_component(var, conf)

    for conf in config[CONF_FANS]:
        var = await fan.new_fan(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_type(conf[CONF_TYPE]))

    for conf in config[CONF_LIGHTS]:
        var = await light.new_light(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_type(conf[CONF_TYPE]))

    for conf in config[CONF_LOCKS]:
        var = await lock.new_lock(conf)
        if conf[CONF_TYPE] == 2:
            cg.add(var.traits.set_supports_open(True))

    for conf in config[CONF_NUMBERS]:
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await cg.register_component(var, conf)
        cg.add(var.set_type(conf[CONF_TYPE]))

    for conf in config[CONF_SELECTS]:
        var = await select.new_select(conf, options=conf[CONF_OPTIONS])
        await cg.register_component(var, conf)

    for conf in config[CONF_SENSORS]:
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)

    for conf in config[CONF_SWITCHES]:
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)

    for conf in config[CONF_TEXTS]:
        var = await text.new_text(conf)
        await cg.register_component(var, conf)
        if conf[CONF_TYPE] == 2:
            cg.add(var.traits.set_mode(text.TextMode.TEXT_MODE_PASSWORD))

    for conf in config[CONF_TEXT_SENSORS]:
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)

    for conf in config[CONF_VALVES]:
        var = await valve.new_valve(conf)
        cg.add(var.set_type(conf[CONF_TYPE]))
