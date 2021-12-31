from esphome import automation
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import uart, time, switch, sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_ON_CLICK,
    CONF_TEMPERATURE,
    CONF_THEN,
    CONF_TIME_ID,
    CONF_TYPE,
)

AUTO_LOAD = ["json"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["uart", "wifi", "esp32"]


nspanel_ns = cg.esphome_ns.namespace("nspanel")
NSPanel = nspanel_ns.class_("NSPanel", cg.Component, uart.UARTDevice)

Widget = nspanel_ns.struct("Widget")
WidgetType = nspanel_ns.enum("WidgetType")

GroupItem = nspanel_ns.struct("GroupItem")

CONF_ECO_MODE_SWITCH = "eco_mode_switch"
CONF_EMPTY = "empty"
CONF_ITEMS = "items"
CONF_RELAYS = "relays"
CONF_SCREEN_POWER_SWITCH = "screen_power_switch"
CONF_TEMPERATURE_UNIT_CELSIUS = "temperature_unit_celsius"
CONF_UIID = "uiid"
CONF_WIDGETS = "widgets"

DEVICE = "device"
EMPTY = "empty"
GROUP = "group"
SCENE = "scene"

GROUP_WIDGETS = {
    "SWITCH_HORIZONTAL": 1,
    "SWITCH_HORIZONTAL_DOUBLE": 2,
    "SWITCH_HORIZONTAL_TRIPLE": 3,
    "SWITCH_HORIZONTAL_QUAD": 4,
    "SWITCH_VERTICAL": 6,
    "SWITCH_VERTICAL_DOUBLE": 7,
    "SWITCH_VERTICAL_TRIPLE": 8,
    "SWITCH_VERTICAL_QUAD": 9,
}

DEVICE_WIDGETS = {
    "RGB_LIGHT_STRIP": 33,
    "CCT_LIGHT": 52,
    "RGB_CCT_LIGHT": 69,
}

ALL = {**GROUP_WIDGETS, **DEVICE_WIDGETS}


widget_name = cv.All(cv.string_strict, cv.Length(max=7))

GROUP_ITEM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GroupItem),
        cv.Required(CONF_NAME): widget_name,
        cv.Required(CONF_ON_CLICK): automation.validate_automation(single=True),
    }
)

WIDGET_SCHEMA = cv.typed_schema(
    {
        EMPTY: cv.Schema({cv.GenerateID(): cv.declare_id(Widget)}),
        DEVICE: cv.All(
            cv.invalid("Device widgets are not yet supported"),
            cv.Schema(
                {
                    cv.GenerateID(): cv.declare_id(Widget),
                    cv.Required(CONF_NAME): widget_name,
                    cv.Required(CONF_UIID): cv.enum(ALL, upper=True, space="_"),
                    cv.Required(CONF_THEN): automation.validate_automation(single=True),
                }
            ),
        ),
        GROUP: cv.All(
            cv.invalid("Group widgets are not yet supported"),
            cv.Schema(
                {
                    cv.GenerateID(): cv.declare_id(Widget),
                    cv.Required(CONF_NAME): widget_name,
                    cv.Required(CONF_UIID): cv.enum(ALL, upper=True, space="_"),
                    cv.Required(CONF_ITEMS): cv.All(
                        cv.ensure_list(GROUP_ITEM_SCHEMA), cv.Length(min=1)
                    ),
                }
            ),
        ),
        SCENE: cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Widget),
                cv.Required(CONF_NAME): widget_name,
                cv.Required(CONF_ON_CLICK): automation.validate_automation(single=True),
            }
        ),
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NSPanel),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Required(CONF_RELAYS): cv.All(
                cv.ensure_list(cv.use_id(switch.Switch)), cv.Length(min=2, max=2)
            ),
            cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_SCREEN_POWER_SWITCH): cv.use_id(switch.Switch),
            cv.Optional(CONF_TEMPERATURE_UNIT_CELSIUS, default=True): cv.boolean,
            cv.Optional(CONF_ECO_MODE_SWITCH): cv.use_id(switch.Switch),
            cv.Optional(CONF_WIDGETS): cv.All(
                cv.ensure_list(WIDGET_SCHEMA), cv.Length(min=8, max=8)
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))

    relays = []
    for relay_id in config[CONF_RELAYS]:
        relays.append(await cg.get_variable(relay_id))

    cg.add(var.set_relays(*relays))

    sens = await cg.get_variable(config[CONF_TEMPERATURE])
    cg.add(var.set_temperature_sensor(sens))

    cg.add(var.set_temperature_unit_celsius(config[CONF_TEMPERATURE_UNIT_CELSIUS]))

    sceen_power = await cg.get_variable(config.get(CONF_SCREEN_POWER_SWITCH))
    cg.add(var.set_screen_power_switch(sceen_power))

    if CONF_ECO_MODE_SWITCH in config:
        eco_mode = await cg.get_variable(config.get(CONF_ECO_MODE_SWITCH))
        cg.add(var.set_eco_mode_switch(eco_mode))

    if CONF_WIDGETS in config:
        for idx, widget_conf in enumerate(config[CONF_WIDGETS]):
            if widget_conf[CONF_TYPE] == EMPTY:
                widget_struct = cg.new_variable(
                    widget_conf[CONF_ID],
                    cg.StructInitializer(
                        Widget,
                        ("id", idx + 1),
                        ("type", WidgetType.EMPTY),
                    ),
                )
            elif widget_conf[CONF_TYPE] == DEVICE:
                # TODO automation
                widget_struct = cg.new_variable(
                    widget_conf[CONF_ID],
                    cg.StructInitializer(
                        Widget,
                        ("id", idx + 1),
                        ("type", WidgetType.DEVICE),
                        ("name", widget_conf[CONF_NAME]),
                        ("uiid", widget_conf[CONF_UIID]),
                    ),
                )
            elif widget_conf[CONF_TYPE] == GROUP:
                items = []
                for item_idx, item_conf in enumerate(widget_conf[CONF_ITEMS]):
                    item = cg.new_variable(
                        item_conf[CONF_ID],
                        cg.StructInitializer(
                            GroupItem,
                            ("id", item_idx),
                            ("widget_id", idx + 1),
                            ("name", item_conf[CONF_NAME]),
                        ),
                    )
                    await automation.build_automation(
                        item.trigger, [(bool, "x")], item_conf[CONF_ON_CLICK]
                    )
                    items.append(item)

                widget_struct = cg.new_variable(
                    widget_conf[CONF_ID],
                    cg.StructInitializer(
                        Widget,
                        ("id", idx + 1),
                        ("type", WidgetType.GROUP),
                        ("name", widget_conf[CONF_NAME]),
                        ("uiid", widget_conf[CONF_UIID]),
                        ("items", items),
                    ),
                )
            elif widget_conf[CONF_TYPE] == SCENE:
                widget_struct = cg.new_variable(
                    widget_conf[CONF_ID],
                    cg.StructInitializer(
                        Widget,
                        ("id", idx + 1),
                        ("type", WidgetType.SCENE),
                        ("name", widget_conf[CONF_NAME]),
                    ),
                )
                await automation.build_automation(
                    widget_struct.trigger, [], widget_conf[CONF_ON_CLICK]
                )

            cg.add(var.set_widget(idx, widget_struct))

    cg.add_define("USE_NSPANEL")
