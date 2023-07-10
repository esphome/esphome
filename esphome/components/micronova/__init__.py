import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.components import button
from esphome.components import switch
from esphome.components import sensor
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CODEOWNERS = ["@jorre05"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor", "button", "switch"]

CONF_ENABLE_RX_PIN = "enable_rx_pin"
CONF_SCAN_MEMORY_LOCATION = "scan_memory_location"
CONF_MEMORY_LOCATION = "memory_location"
CONF_MEMORY_ADDRESS = "memory_address"
CONF_MEMORY_DATA = "memory_data"
CONF_MEMORY_DATA_ON = "memory_data_on"
CONF_MEMORY_DATA_OFF = "memory_data_off"
CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"
CONF_FUMES_TEMPERATURE = "fumes_temperature"
CONF_STOVE_POWER = "stove_power"
CONF_FAN_SPEED = "fan_speed"
CONF_MEMORY_ADDRESS_SENSOR = "memory_address_sensor"
CONF_FAN_RPM_OFFSET = "fan_rpm_offset"
CONF_STOVE_STATE = "stove_state"
CONF_STOVE_SWITCH = "stove_switch"
CONF_BUT_TEMP_UP = "but_temp_up"
CONF_BUT_TEMP_DOWN = "but_temp_down"

ICON_STATE = "mdi:checkbox-marked-circle-outline"

UNIT_RPM = "rpm"

SENSORS = [
    CONF_ROOM_TEMPERATURE,
    CONF_THERMOSTAT_TEMPERATURE,
    CONF_FUMES_TEMPERATURE,
    CONF_STOVE_POWER,
    CONF_FAN_SPEED,
    CONF_MEMORY_ADDRESS_SENSOR,
]

TEXT_SENSORS = [
    CONF_STOVE_STATE,
]

SWITCHES = [
    CONF_STOVE_SWITCH,
]

BUTTONS = [
    CONF_BUT_TEMP_UP,
    CONF_BUT_TEMP_DOWN,
]

micronova_ns = cg.esphome_ns.namespace("micronova")

MicroNovaFunctions = micronova_ns.enum("MicroNovaFunctions", is_class=True)
MICRONOVA_FUNCTIONS_ENUM = {
    "STOVE_FUNCTION_SWITCH": MicroNovaFunctions.STOVE_FUNCTION_SWITCH,
    "STOVE_FUNCTION_TEMP_UP": MicroNovaFunctions.STOVE_FUNCTION_TEMP_UP,
    "STOVE_FUNCTION_TEMP_DOWN": MicroNovaFunctions.STOVE_FUNCTION_TEMP_DOWN,
    "STOVE_FUNCTION_ROOM_TEMPERATURE": MicroNovaFunctions.STOVE_FUNCTION_ROOM_TEMPERATURE,
    "STOVE_FUNCTION_THERMOSTAT_TEMPERATURE": MicroNovaFunctions.STOVE_FUNCTION_THERMOSTAT_TEMPERATURE,
    "STOVE_FUNCTION_FUMES_TEMPERATURE": MicroNovaFunctions.STOVE_FUNCTION_FUMES_TEMPERATURE,
    "STOVE_FUNCTION_STOVE_POWER": MicroNovaFunctions.STOVE_FUNCTION_STOVE_POWER,
    "STOVE_FUNCTION_FAN_SPEED": MicroNovaFunctions.STOVE_FUNCTION_FAN_SPEED,
    "STOVE_FUNCTION_STOVE_STATE": MicroNovaFunctions.STOVE_FUNCTION_STOVE_STATE,
    "STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR": MicroNovaFunctions.STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR,
}

MicroNova = micronova_ns.class_(
    "MicroNova", cg.PollingComponent, uart.UARTDevice, switch.Switch
)
MicroNovaSwitch = micronova_ns.class_("MicroNovaSwitch", switch.Switch, cg.Component)
MicroNovaButton = micronova_ns.class_("MicroNovaButton", button.Button, cg.Component)
MicroNovaSensor = micronova_ns.class_("MicroNovaSensor", button.Button, cg.Component)
MicroNovaTextSensor = micronova_ns.class_(
    "MicroNovaTextSensor", button.Button, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MicroNova),
            cv.Required(CONF_ENABLE_RX_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SCAN_MEMORY_LOCATION): cv.hex_int_range(),
            cv.Optional(CONF_ROOM_TEMPERATURE): sensor.sensor_schema(
                MicroNovaSensor,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x01): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0x00): cv.hex_int_range()}
            ),
            cv.Optional(CONF_THERMOSTAT_TEMPERATURE): sensor.sensor_schema(
                MicroNovaSensor,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x7D): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0x20): cv.hex_int_range()}
            ),
            cv.Optional(CONF_FUMES_TEMPERATURE): sensor.sensor_schema(
                MicroNovaSensor,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x5A): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0x00): cv.hex_int_range()}
            ),
            cv.Optional(CONF_STOVE_POWER): sensor.sensor_schema(
                MicroNovaSensor,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x34): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0x00): cv.hex_int_range()}
            ),
            cv.Optional(CONF_FAN_SPEED): sensor.sensor_schema(
                MicroNovaSensor,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement=UNIT_RPM,
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x37): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0x00): cv.hex_int_range()}
            )
            .extend(
                {
                    cv.Optional(CONF_FAN_RPM_OFFSET, default=0): cv.int_range(
                        min=0, max=255
                    )
                }
            ),
            cv.Optional(CONF_MEMORY_ADDRESS_SENSOR): sensor.sensor_schema(
                MicroNovaSensor,
                accuracy_decimals=1,
            )
            .extend({cv.Required(CONF_MEMORY_ADDRESS): cv.hex_int_range()})
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0x00): cv.hex_int_range()}
            ),
            cv.Optional(CONF_STOVE_STATE): text_sensor.text_sensor_schema(
                MicroNovaTextSensor, icon=ICON_STATE
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x21): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0x00): cv.hex_int_range()}
            ),
            cv.Optional(CONF_STOVE_SWITCH): switch.switch_schema(
                MicroNovaSwitch,
                icon=ICON_STATE,
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x21): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0x80): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_DATA_OFF, default=0x06): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_DATA_ON, default=0x01): cv.hex_int_range()}
            ),
            cv.Optional(CONF_BUT_TEMP_UP): button.button_schema(
                MicroNovaButton,
                icon=ICON_STATE,
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x7D): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0xA0): cv.hex_int_range()}
            )
            .extend({cv.Optional(CONF_MEMORY_DATA, default=0x00): cv.hex_int_range()}),
            cv.Optional(CONF_BUT_TEMP_DOWN): button.button_schema(
                MicroNovaButton,
                icon=ICON_STATE,
            )
            .extend(
                {cv.Optional(CONF_MEMORY_ADDRESS, default=0x7D): cv.hex_int_range()}
            )
            .extend(
                {cv.Optional(CONF_MEMORY_LOCATION, default=0xA0): cv.hex_int_range()}
            )
            .extend({cv.Optional(CONF_MEMORY_DATA, default=0x00): cv.hex_int_range()}),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("60s")),
    cv.only_with_arduino,
)


async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    micronova_var = cg.new_Pvariable(config[CONF_ID], uart_component)
    mv = await cg.get_variable(config[CONF_ID])
    await cg.register_component(micronova_var, config)

    enable_rx_pin = await cg.gpio_pin_expression(config[CONF_ENABLE_RX_PIN])
    cg.add(micronova_var.set_enable_rx_pin(enable_rx_pin))

    if CONF_SCAN_MEMORY_LOCATION in config:
        cg.add(
            micronova_var.set_scan_memory_location(config[CONF_SCAN_MEMORY_LOCATION])
        )

    for key in SWITCHES:
        if key in config:
            conf = config[key]
            sw = await switch.new_switch(conf, mv)
            await cg.register_component(sw, conf)
            cg.add(micronova_var.set_stove_switch(sw))
            cg.add(sw.set_memory_location(conf[CONF_MEMORY_LOCATION]))
            cg.add(sw.set_memory_address(conf[CONF_MEMORY_ADDRESS]))
            cg.add(sw.set_memory_data_on(conf[CONF_MEMORY_DATA_ON]))
            cg.add(sw.set_memory_data_off(conf[CONF_MEMORY_DATA_OFF]))
            if key == CONF_STOVE_SWITCH:
                cg.add(sw.set_function(MicroNovaFunctions.STOVE_FUNCTION_SWITCH))

    for key in BUTTONS:
        if key in config:
            conf = config[key]
            bt = await button.new_button(conf, mv)
            await cg.register_component(bt, conf)
            cg.add(bt.set_memory_location(conf[CONF_MEMORY_LOCATION]))
            cg.add(bt.set_memory_address(conf[CONF_MEMORY_ADDRESS]))
            cg.add(bt.set_memory_data(conf[CONF_MEMORY_DATA]))
            if key == CONF_BUT_TEMP_UP:
                cg.add(micronova_var.set_temp_up_button(bt))
                cg.add(bt.set_function(MicroNovaFunctions.STOVE_FUNCTION_TEMP_UP))
            if key == CONF_BUT_TEMP_DOWN:
                cg.add(micronova_var.set_temp_down_button(bt))
                cg.add(bt.set_function(MicroNovaFunctions.STOVE_FUNCTION_TEMP_DOWN))

    for key in SENSORS:
        if key in config:
            conf = config[key]
            sens = await sensor.new_sensor(conf, mv)
            cg.add(micronova_var.add_sensor(sens))
            cg.add(sens.set_memory_location(conf[CONF_MEMORY_LOCATION]))
            cg.add(sens.set_memory_address(conf[CONF_MEMORY_ADDRESS]))
            if key == CONF_ROOM_TEMPERATURE:
                cg.add(
                    sens.set_function(
                        MicroNovaFunctions.STOVE_FUNCTION_ROOM_TEMPERATURE
                    )
                )
            if key == CONF_THERMOSTAT_TEMPERATURE:
                cg.add(
                    sens.set_function(
                        MicroNovaFunctions.STOVE_FUNCTION_THERMOSTAT_TEMPERATURE
                    )
                )
            if key == CONF_FUMES_TEMPERATURE:
                cg.add(
                    sens.set_function(
                        MicroNovaFunctions.STOVE_FUNCTION_FUMES_TEMPERATURE
                    )
                )
            if key == CONF_STOVE_POWER:
                cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_STOVE_POWER))
            if key == CONF_FAN_SPEED:
                cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_FAN_SPEED))
                cg.add(sens.set_fan_speed_offset(conf[CONF_FAN_RPM_OFFSET]))
            if key == CONF_MEMORY_ADDRESS_SENSOR:
                cg.add(
                    sens.set_function(
                        MicroNovaFunctions.STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR
                    )
                )

    for key in TEXT_SENSORS:
        if key in config:
            conf = config[key]
            sens = await text_sensor.new_text_sensor(conf, mv)
            cg.add(micronova_var.add_sensor(sens))
            cg.add(sens.set_memory_location(conf[CONF_MEMORY_LOCATION]))
            cg.add(sens.set_memory_address(conf[CONF_MEMORY_ADDRESS]))
            if key == CONF_STOVE_STATE:
                cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_STOVE_STATE))
