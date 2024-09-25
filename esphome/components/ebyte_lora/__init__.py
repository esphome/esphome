import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome import pins
from esphome.components.binary_sensor import BinarySensor
from esphome.components.sensor import Sensor
from esphome.components import uart

from esphome.const import (
    CONF_BINARY_SENSORS,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    UNIT_PERCENT,
    CONF_CHANNEL,
    CONF_SENSORS,
)
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@danielkoek"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["uart"]
MULTI_CONF = True

ebyte_lora_ns = cg.esphome_ns.namespace("ebyte_lora")
EbyteLoraComponent = ebyte_lora_ns.class_(
    "EbyteLoraComponent", cg.PollingComponent, uart.UARTDevice
)
CONF_REMOTE_ID = "remote_id"
WorPeriod = ebyte_lora_ns.enum("WorPeriod")
WOR_PERIOD_OPTIONS = {
    "WOR_500": WorPeriod.WOR_500,
    "WOR_1000": WorPeriod.WOR_1000,
    "WOR_1500": WorPeriod.WOR_1500,
    "WOR_2000": WorPeriod.WOR_2000,
    "WOR_2500": WorPeriod.WOR_2500,
    "WOR_3000": WorPeriod.WOR_3000,
    "WOR_3500": WorPeriod.WOR_3500,
    "WOR_4000": WorPeriod.WOR_4000,
}
UartParitySetting = ebyte_lora_ns.enum("UartParitySetting")
UART_PARITY_OPTIONS = {
    "EBYTE_UART_8N1": UartParitySetting.EBYTE_UART_8N1,
    "EBYTE_UART_8O1": UartParitySetting.EBYTE_UART_8O1,
    "EBYTE_UART_8E1": UartParitySetting.EBYTE_UART_8E1,
}
UartBpsSpeed = ebyte_lora_ns.enum("UartBpsSpeed")
UART_BPS_OPTIONS = {
    "UART_1200": UartBpsSpeed.UART_1200,
    "UART_2400": UartBpsSpeed.UART_2400,
    "UART_4800": UartBpsSpeed.UART_4800,
    "UART_9600": UartBpsSpeed.UART_9600,
    "UART_19200": UartBpsSpeed.UART_19200,
    "UART_38400": UartBpsSpeed.UART_38400,
    "UART_57600": UartBpsSpeed.UART_57600,
    "UART_115200": UartBpsSpeed.UART_115200,
}
SubPacketSetting = ebyte_lora_ns.enum("SubPacketSetting")
SUB_PACKET_OPTIONS = {
    "SUB_200B": SubPacketSetting.SUB_200B,
    "SUB_128B": SubPacketSetting.SUB_128B,
    "SUB_64B": SubPacketSetting.SUB_64B,
    "SUB_32B": SubPacketSetting.SUB_32B,
}
TransmissionMode = ebyte_lora_ns.enum("TransmissionMode")
TRANSMISSION_MODE_OPTIONS = {
    "TRANSPARENT": TransmissionMode.TRANSPARENT,
    "FIXED": TransmissionMode.FIXED,
}
TransmissionPower = ebyte_lora_ns.enum("TransmissionPower")
TRANSMISSION_POWER_OPTIONS = {
    "TX_DEFAULT_MAX": TransmissionPower.TX_DEFAULT_MAX,
    "TX_LOWER": TransmissionPower.TX_LOWER,
    "TX_EVEN_LOWER": TransmissionPower.TX_EVEN_LOWER,
    "TX_LOWEST": TransmissionPower.TX_LOWEST,
}

AirDataRate = ebyte_lora_ns.enum("AirDataRate")
AIR_DATA_RATE_OPTIONS = {
    "AIR_2_4KB": AirDataRate.AIR_2_4KB,
    "AIR_4_8KB": AirDataRate.AIR_4_8KB,
    "AIR_9_6KB": AirDataRate.AIR_9_6KB,
    "AIR_19_2KB": AirDataRate.AIR_19_2KB,
    "AIR_38_4KB": AirDataRate.AIR_38_4KB,
    "AIR_62_5KB": AirDataRate.AIR_62_5KB,
}
EnableByte = ebyte_lora_ns.enum("EnableByte")
ENABLE_OPTIONS = {
    "EBYTE_ENABLED": EnableByte.EBYTE_ENABLED,
    "EBYTE_DISABLED": EnableByte.EBYTE_DISABLED,
}


CONF_EBYTE_LORA_COMPONENT_ID = "ebyte_lora_component_id"
CONF_BROADCAST_ID = "broadcast_id"
CONF_PIN_AUX = "pin_aux"
CONF_PIN_M0 = "pin_m0"
CONF_PIN_M1 = "pin_m1"
CONF_LORA_RSSI = "lora_rssi"
CONF_ADDL = "addl"
CONF_ADDH = "addh"
CONF_UART_BPS = "uart_bps"
CONF_TRANSMISSION_MODE = "transmission_mode"
CONF_TRANSMISSION_POWER = "transmission_power"
CONF_AIR_DATA_RATE = "air_data_rate"
CONF_WOR_PERIOD = "wor_period"
CONF_ENABLE_RSSI = "enable_rssi"
CONF_ENABLE_LBT = "enable_lbt"
CONF_RSSI_NOISE = "rssi_noise"
CONF_UART_PARITY = "uart_parity"
CONF_SUB_PACKET = "sub_packet"
CONF_SENT_SWITCH_STATE = "sent_switch_state"
CONF_REPEATER = "repeater"
CONF_NETWORK_ID = "network_id"


def sensor_validation(cls: MockObjClass):
    return cv.maybe_simple_value(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(cls),
                cv.Optional(CONF_BROADCAST_ID): cv.validate_id_name,
            }
        ),
        key=CONF_ID,
    )


def require_internal_with_name(config):
    if CONF_NAME in config and CONF_INTERNAL not in config:
        raise cv.Invalid("Must provide internal: config when using name:")
    return config


CONFIG_SCHEMA = cv.All(
    cv.polling_component_schema("15s")
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(EbyteLoraComponent),
            # for communication to let us know that we can receive data
            cv.Required(CONF_PIN_AUX): pins.internal_gpio_input_pin_schema,
            # for communication set the mode
            cv.Required(CONF_PIN_M0): pins.internal_gpio_output_pin_schema,
            # for communication set the mode
            cv.Required(CONF_PIN_M1): pins.internal_gpio_output_pin_schema,
            # to be able to repeater hop
            cv.Required(CONF_NETWORK_ID): cv.int_range(min=0, max=255),
            cv.Optional(CONF_REPEATER, default=False): cv.boolean,
            cv.Optional(CONF_SENSORS): cv.ensure_list(sensor_validation(Sensor)),
            cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(
                sensor_validation(BinarySensor)
            ),
            # if you want to see the rssi
            cv.Optional(CONF_LORA_RSSI): sensor.sensor_schema(
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ADDH, default=0): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ADDL, default=0): cv.int_range(min=0, max=255),
            cv.Optional(CONF_CHANNEL, default=13): cv.int_range(min=0, max=83),
            cv.Optional(CONF_UART_PARITY, default="EBYTE_UART_8N1"): cv.enum(
                UART_PARITY_OPTIONS, upper=True
            ),
            cv.Optional(CONF_UART_BPS, default="UART_9600"): cv.enum(
                UART_BPS_OPTIONS, upper=True
            ),
            cv.Optional(CONF_UART_PARITY, default="EBYTE_UART_8N1"): cv.enum(
                UART_PARITY_OPTIONS, upper=True
            ),
            cv.Optional(CONF_SUB_PACKET, default="SUB_200B"): cv.enum(
                SUB_PACKET_OPTIONS, upper=True
            ),
            cv.Optional(CONF_TRANSMISSION_MODE, default="TRANSPARENT"): cv.enum(
                TRANSMISSION_MODE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_TRANSMISSION_POWER, default="TX_DEFAULT_MAX"): cv.enum(
                TRANSMISSION_POWER_OPTIONS, upper=True
            ),
            cv.Optional(CONF_WOR_PERIOD, default="WOR_4000"): cv.enum(
                WOR_PERIOD_OPTIONS, upper=True
            ),
            cv.Optional(CONF_AIR_DATA_RATE, default="AIR_2_4KB"): cv.enum(
                AIR_DATA_RATE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_ENABLE_RSSI, default="EBYTE_DISABLED"): cv.enum(
                ENABLE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_ENABLE_LBT, default="EBYTE_DISABLED"): cv.enum(
                ENABLE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_RSSI_NOISE, default="EBYTE_DISABLED"): cv.enum(
                ENABLE_OPTIONS, upper=True
            ),
        }
    )
)

SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REMOTE_ID): cv.string_strict,
        cv.Required(CONF_NETWORK_ID): cv.int_range(min=0, max=255),
        cv.GenerateID(CONF_EBYTE_LORA_COMPONENT_ID): cv.use_id(EbyteLoraComponent),
    }
)


async def to_code(config):
    cg.add_global(ebyte_lora_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    pin_aux = await cg.gpio_pin_expression(config[CONF_PIN_AUX])
    cg.add(var.set_pin_aux(pin_aux))
    pin_m0 = await cg.gpio_pin_expression(config[CONF_PIN_M0])
    cg.add(var.set_pin_m0(pin_m0))
    pin_m1 = await cg.gpio_pin_expression(config[CONF_PIN_M1])
    cg.add(var.set_pin_m1(pin_m1))
    cg.add(var.set_repeater(config[CONF_REPEATER]))
    cg.add(var.set_network_id(config[CONF_NETWORK_ID]))
    cg.add(var.set_addh(config[CONF_ADDH]))
    cg.add(var.set_addl(config[CONF_ADDL]))
    cg.add(var.set_air_data_rate(config[CONF_AIR_DATA_RATE]))
    cg.add(var.set_uart_parity(config[CONF_UART_PARITY]))
    cg.add(var.set_uart_bps(config[CONF_UART_BPS]))
    cg.add(var.set_transmission_power(config[CONF_TRANSMISSION_POWER]))
    cg.add(var.set_rssi_noise(config[CONF_RSSI_NOISE]))
    cg.add(var.set_sub_packet(config[CONF_SUB_PACKET]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_wor(config[CONF_WOR_PERIOD]))
    cg.add(var.set_enable_lbt(config[CONF_ENABLE_LBT]))
    cg.add(var.set_transmission_mode(config[CONF_TRANSMISSION_MODE]))
    cg.add(var.set_enable_rssi(config[CONF_ENABLE_RSSI]))
    for sens_conf in config.get(CONF_SENSORS, ()):
        sens_id = sens_conf[CONF_ID]
        sensor_device = await cg.get_variable(sens_id)
        bcst_id = sens_conf.get(CONF_BROADCAST_ID, sens_id.id)
        cg.add(var.add_sensor(bcst_id, sensor_device))
    for sens_conf in config.get(CONF_BINARY_SENSORS, ()):
        sens_id = sens_conf[CONF_ID]
        sensor_device = await cg.get_variable(sens_id)
        bcst_id = sens_conf.get(CONF_BROADCAST_ID, sens_id.id)
        cg.add(var.add_binary_sensor(bcst_id, sensor_device))
    if CONF_LORA_RSSI in config:
        sensor_device = await sensor.new_sensor(config[CONF_LORA_RSSI])
        cg.add(var.set_rssi_sensor(sensor_device))
