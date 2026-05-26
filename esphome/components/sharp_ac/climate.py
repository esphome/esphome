import esphome.codegen as cg
from esphome.components import button, climate, select, switch, text_sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["button", "select", "switch", "text_sensor"]
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@sven819"]

sharp_ac_ns = cg.esphome_ns.namespace("sharp_ac")
SharpAc = sharp_ac_ns.class_("SharpAc", climate.Climate, uart.UARTDevice, cg.Component)

VaneSelectVertical = sharp_ac_ns.class_(
    "VaneSelectVertical", select.Select, cg.Component
)
VaneSelectHorizontal = sharp_ac_ns.class_(
    "VaneSelectHorizontal", select.Select, cg.Component
)

IonSwitch = sharp_ac_ns.class_("IonSwitch", switch.Switch)
ConnectionStatusSensor = sharp_ac_ns.class_(
    "ConnectionStatusSensor", text_sensor.TextSensor
)
ReconnectButton = sharp_ac_ns.class_("ReconnectButton", button.Button)

CONF_VANE = "vane"
CONF_HORIZONTAL = "horizontal"
CONF_VERTICAL = "vertical"
CONF_ION_SWITCH = "ion_switch"
CONF_CONNECTION_STATUS = "connection_status"
CONF_RECONNECT_BUTTON = "reconnect_button"

HORIZONTAL_SWING_OPTIONS = ["swing", "left", "center", "right"]
VERTICAL_SWING_OPTIONS = [
    "auto",
    "swing",
    "up",
    "up_center",
    "center",
    "down_center",
    "down",
]

SELECT_SCHEMA_VERTICAL = select.select_schema(VaneSelectVertical).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(VaneSelectVertical),
    }
)

SELECT_SCHEMA_HORIZONTAL = select.select_schema(VaneSelectHorizontal).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(VaneSelectHorizontal),
    }
)

VANE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_HORIZONTAL): SELECT_SCHEMA_HORIZONTAL,
        cv.Optional(CONF_VERTICAL): SELECT_SCHEMA_VERTICAL,
    }
)

ION_SCHEMA = switch.switch_schema(IonSwitch).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(IonSwitch)}
)

CONNECTION_STATUS_SCHEMA = text_sensor.text_sensor_schema(
    ConnectionStatusSensor
).extend({cv.GenerateID(CONF_ID): cv.declare_id(ConnectionStatusSensor)})

RECONNECT_BUTTON_SCHEMA = button.button_schema(ReconnectButton).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(ReconnectButton)}
)

CONFIG_SCHEMA = (
    climate.climate_schema(SharpAc)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SharpAc),
            cv.Optional(CONF_VANE): VANE_SCHEMA,
            cv.Optional(CONF_ION_SWITCH): ION_SCHEMA,
            cv.Optional(CONF_CONNECTION_STATUS): CONNECTION_STATUS_SCHEMA,
            cv.Optional(CONF_RECONNECT_BUTTON): RECONNECT_BUTTON_SCHEMA,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "sharp_ac",
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    parity="EVEN",
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    vane_config = config.get(CONF_VANE, {})

    if horizontal_conf := vane_config.get(CONF_HORIZONTAL):
        swing_select = await select.new_select(
            horizontal_conf, options=HORIZONTAL_SWING_OPTIONS
        )
        await cg.register_parented(swing_select, var)
        cg.add(var.setVaneHorizontalSelect(swing_select))

    if vertical_conf := vane_config.get(CONF_VERTICAL):
        swing_select = await select.new_select(
            vertical_conf, options=VERTICAL_SWING_OPTIONS
        )
        await cg.register_parented(swing_select, var)
        cg.add(var.setVaneVerticalSelect(swing_select))

    if CONF_ION_SWITCH in config:
        conf = config[CONF_ION_SWITCH]
        swt = await switch.new_switch(conf)
        await cg.register_parented(swt, var)
        cg.add(var.setIonSwitch(swt))

    if CONF_CONNECTION_STATUS in config:
        conf = config[CONF_CONNECTION_STATUS]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(var.setConnectionStatusSensor(sens))

    if CONF_RECONNECT_BUTTON in config:
        conf = config[CONF_RECONNECT_BUTTON]
        btn = await button.new_button(conf)
        await cg.register_parented(btn, var)
        cg.add(var.setReconnectButton(btn))

    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
    await cg.register_component(var, config)
