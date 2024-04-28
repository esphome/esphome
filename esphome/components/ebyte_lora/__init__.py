import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, uart

from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    CONF_ID,
    UNIT_PERCENT,
    CONF_CHANNEL,
)

CODEOWNERS = ["@danielkoek"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["uart", "sensor"]
MULTI_CONF = True

ebyte_lora_ns = cg.esphome_ns.namespace("ebyte_lora")
EbyteLoraComponent = ebyte_lora_ns.class_(
    "EbyteLoraComponent", cg.PollingComponent, uart.UARTDevice
)
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


CONF_EBYTE_LORA = "ebyte_lora"
CONF_PIN_AUX = "pin_aux"
CONF_PIN_M0 = "pin_m0"
CONF_PIN_M1 = "pin_m1"
CONF_LORA_RSSI = "lora_rssi"
CONF_UART_BPS = "uart_bps"
CONF_TRANSMISSION_MODE = "transmission_mode"
CONF_TRANSMISSION_POWER = "transmission_power"
CONF_AIR_DATA_RATE = "air_data_rate"
CONF_WOR_PERIOD = "wor_period"
CONF_ENABLE_RSSI = "enable_rssi"
CONF_ENABLE_LBT = "enable_lbt"
CONF_RSSI_NOISE = "rssi_noise"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EbyteLoraComponent),
            # for communication to let us know that we can receive data
            cv.Required(CONF_PIN_AUX): pins.internal_gpio_input_pin_schema,
            # for communication set the mode
            cv.Required(CONF_PIN_M0): pins.internal_gpio_output_pin_schema,
            # for communication set the mode
            cv.Required(CONF_PIN_M1): pins.internal_gpio_output_pin_schema,
            # if you want to see the rssi
            cv.Optional(CONF_LORA_RSSI): sensor.sensor_schema(
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CHANNEL, default=13): cv.int_range(min=0, max=83),
            cv.Optional(CONF_UART_BPS, default="UART_9600"): cv.enum(
                UART_BPS_OPTIONS, upper=True
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
    .extend(cv.polling_component_schema("20s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    pin_aux = await cg.gpio_pin_expression(config[CONF_PIN_AUX])
    cg.add(var.set_pin_aux(pin_aux))

    pin_m0 = await cg.gpio_pin_expression(config[CONF_PIN_M0])
    cg.add(var.set_pin_m0(pin_m0))
    pin_m1 = await cg.gpio_pin_expression(config[CONF_PIN_M1])
    cg.add(var.set_pin_m1(pin_m1))
    cg.add(var.set_uart_bps(config[CONF_UART_BPS]))
    cg.add(var.set_transmission_mode(config[CONF_TRANSMISSION_MODE]))
    cg.add(var.set_transmission_power(config[CONF_TRANSMISSION_POWER]))
    cg.add(var.set_air_data_rate(config[CONF_AIR_DATA_RATE]))
    cg.add(var.set_enable_rssi(config[CONF_ENABLE_RSSI]))
    cg.add(var.set_enable_lbt(config[CONF_ENABLE_LBT]))
    cg.add(var.set_rssi_noise(config[CONF_RSSI_NOISE]))
    cg.add(var.set_wor(config[CONF_WOR_PERIOD]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))

    if CONF_LORA_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_LORA_RSSI])
        cg.add(var.set_rssi_sensor(sens))
