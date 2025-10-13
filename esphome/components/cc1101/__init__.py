import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.log import Fore, color
from esphome.components import time
from esphome.components import mqtt
from esphome.components import wifi
from esphome.components import ethernet
from esphome.components.network import IPAddress
from esphome.const import (
    CONF_ID,
    CONF_MOSI_PIN,
    CONF_MISO_PIN,
    CONF_CLK_PIN,
    CONF_CS_PIN,
    CONF_NAME,
    CONF_IP_ADDRESS,
    CONF_PORT,
    CONF_FORMAT,
    CONF_TIME_ID,
    CONF_FREQUENCY,
    CONF_MQTT_ID,
    CONF_MQTT,
    CONF_BROKER,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_RETAIN,
)

from esphome.const import SOURCE_FILE_EXTENSIONS

CONF_TRANSPORT = "transport"

CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO2_PIN = "gdo2_pin"
CONF_LED_PIN = "led_pin"
CONF_LED_BLINK_TIME = "led_blink_time"
CONF_LOG_ALL = "log_all"
CONF_ALL_DRIVERS = "all_drivers"
CONF_SYNC_MODE = "sync_mode"
CONF_INFO_COMP_ID = "info_comp_id"
CONF_WMBUS_MQTT_ID = "wmbus_mqtt_id"
CONF_WMBUS_MQTT_RAW = "mqtt_raw"
CONF_WMBUS_MQTT_RAW_PREFIX = "mqtt_raw_prefix"
CONF_WMBUS_MQTT_RAW_PARSED = "mqtt_raw_parsed"
CONF_WMBUS_MQTT_RAW_FORMAT = "mqtt_raw_format"
CONF_CLIENTS = 'clients'
CONF_ETH_REF = "wmbus_eth_id"
CONF_WIFI_REF = "wmbus_wifi_id"

CODEOWNERS = ["@SzczepanLeon"]

DEPENDENCIES = ["time"]
AUTO_LOAD = ["sensor", "text_sensor"]

wmbus_ns = cg.esphome_ns.namespace('wmbus')
WMBusComponent = wmbus_ns.class_('WMBusComponent', cg.Component)
InfoComponent = wmbus_ns.class_('InfoComponent', cg.Component)
Client = wmbus_ns.struct('Client')
Format = wmbus_ns.enum("Format")
RawFormat = wmbus_ns.enum("RawFormat")
Transport = wmbus_ns.enum("Transport")
MqttClient = wmbus_ns.struct('MqttClient')

FORMAT = {
    "HEX": Format.FORMAT_HEX,
    "RTLWMBUS": Format.FORMAT_RTLWMBUS,
}
validate_format = cv.enum(FORMAT, upper=True)

RAW_FORMAT = {
    "JSON": RawFormat.RAW_FORMAT_JSON,
    "RTLWMBUS": RawFormat.RAW_FORMAT_RTLWMBUS,
}
validate_raw_format = cv.enum(RAW_FORMAT, upper=True)

TRANSPORT = {
    "TCP": Transport.TRANSPORT_TCP,
    "UDP": Transport.TRANSPORT_UDP,
}
validate_transport = cv.enum(TRANSPORT, upper=True)

CLIENT_SCHEMA = cv.Schema({
    cv.GenerateID():                              cv.declare_id(Client),
    cv.Required(CONF_NAME):                       cv.string_strict,
    cv.Required(CONF_IP_ADDRESS):                 cv.ipv4address,
    cv.Required(CONF_PORT):                       cv.port,
    cv.Optional(CONF_TRANSPORT, default="TCP"):   cv.templatable(validate_transport),
    cv.Optional(CONF_FORMAT, default="RTLWMBUS"): cv.templatable(validate_format),
})

WMBUS_MQTT_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_WMBUS_MQTT_ID):        cv.declare_id(MqttClient),
    cv.Required(CONF_USERNAME):               cv.string_strict,
    cv.Required(CONF_PASSWORD):               cv.string_strict,
    cv.Required(CONF_BROKER):                 cv.ipv4address,
    cv.Optional(CONF_PORT,    default=1883):  cv.port,
    cv.Optional(CONF_RETAIN,  default=False): cv.boolean,
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_INFO_COMP_ID):                  cv.declare_id(InfoComponent),
    cv.GenerateID():                                   cv.declare_id(WMBusComponent),
    cv.OnlyWith(CONF_MQTT_ID, "mqtt"):                 cv.use_id(mqtt.MQTTClientComponent),
    cv.OnlyWith(CONF_TIME_ID, "time"):                 cv.use_id(time.RealTimeClock),
    cv.OnlyWith(CONF_WIFI_REF, "wifi"):                cv.use_id(wifi.WiFiComponent),
    cv.OnlyWith(CONF_ETH_REF, "ethernet"):             cv.use_id(ethernet.EthernetComponent),
    cv.Optional(CONF_MOSI_PIN,       default=13):      pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_MISO_PIN,       default=12):      pins.internal_gpio_input_pin_schema,
    cv.Optional(CONF_CLK_PIN,        default=14):      pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_CS_PIN,         default=2):       pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_GDO0_PIN,       default=5):       pins.internal_gpio_input_pin_schema,
    cv.Optional(CONF_GDO2_PIN,       default=4):       pins.internal_gpio_input_pin_schema,
    cv.Optional(CONF_LED_PIN):                         pins.gpio_output_pin_schema,
    cv.Optional(CONF_LED_BLINK_TIME, default="200ms"): cv.positive_time_period,
    cv.Optional(CONF_LOG_ALL,        default=False):   cv.boolean,
    cv.Optional(CONF_ALL_DRIVERS,    default=False):   cv.boolean,
    cv.Optional(CONF_CLIENTS):                         cv.ensure_list(CLIENT_SCHEMA),
    cv.Optional(CONF_FREQUENCY,      default=868.950): cv.float_range(min=300, max=928),
    cv.Optional(CONF_SYNC_MODE,      default=False):   cv.boolean,
    cv.Optional(CONF_MQTT):                            cv.ensure_schema(WMBUS_MQTT_SCHEMA),
    cv.Optional(CONF_WMBUS_MQTT_RAW, default=False): cv.boolean,
    cv.Optional(CONF_WMBUS_MQTT_RAW_PREFIX, default=""): cv.string,
    cv.Optional(CONF_WMBUS_MQTT_RAW_FORMAT, default="JSON"): cv.templatable(validate_raw_format),
    cv.Optional(CONF_WMBUS_MQTT_RAW_PARSED, default=True): cv.boolean,
})

def safe_ip(ip):
    if ip is None:
        return IPAddress(0, 0, 0, 0)
    return IPAddress(str(ip))

async def to_code(config):
    var_adv = cg.new_Pvariable(config[CONF_INFO_COMP_ID])
    await cg.register_component(var_adv, {})

    if (config.get(CONF_MQTT_ID) and config.get(CONF_MQTT)):
        print(color(Fore.RED, "Only one MQTT can be configured!"))
        exit()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mosi = await cg.gpio_pin_expression(config[CONF_MOSI_PIN])
    miso = await cg.gpio_pin_expression(config[CONF_MISO_PIN])
    clk  = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cs   = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    gdo0 = await cg.gpio_pin_expression(config[CONF_GDO0_PIN])
    gdo2 = await cg.gpio_pin_expression(config[CONF_GDO2_PIN])

    cg.add(var.add_cc1101(mosi, miso, clk, cs, gdo0, gdo2, config[CONF_FREQUENCY], config[CONF_SYNC_MODE]))

    time = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time))


    if config.get(CONF_ETH_REF):
        eth = await cg.get_variable(config[CONF_ETH_REF])
        cg.add(var.set_eth(eth))

    if config.get(CONF_WIFI_REF):
        wifi = await cg.get_variable(config[CONF_WIFI_REF])
        cg.add(var.set_wifi(wifi))

    if config.get(CONF_MQTT_ID):
        mqtt = await cg.get_variable(config[CONF_MQTT_ID])
        cg.add(var.set_mqtt(mqtt))

    if config.get(CONF_MQTT):
        conf = config.get(CONF_MQTT)
        cg.add_define("USE_WMBUS_MQTT")
        cg.add_library("knolleary/PubSubClient", "2.8")
        cg.add(var.set_mqtt(conf[CONF_USERNAME],
                            conf[CONF_PASSWORD],
                            safe_ip(conf[CONF_BROKER]),
                            conf[CONF_PORT],
                            conf[CONF_RETAIN]))

    cg.add(var.set_mqtt_raw(config[CONF_WMBUS_MQTT_RAW]))
    cg.add(var.set_mqtt_raw_prefix(config[CONF_WMBUS_MQTT_RAW_PREFIX]))
    cg.add(var.set_mqtt_raw_format(config[CONF_WMBUS_MQTT_RAW_FORMAT]))
    cg.add(var.set_mqtt_raw_parsed(config[CONF_WMBUS_MQTT_RAW_PARSED]))

    cg.add(var.set_log_all(config[CONF_LOG_ALL]))

    for conf in config.get(CONF_CLIENTS, []):
        cg.add(var.add_client(conf[CONF_NAME],
                              safe_ip(conf[CONF_IP_ADDRESS]),
                              conf[CONF_PORT],
                              conf[CONF_TRANSPORT],
                              conf[CONF_FORMAT]))

    if CONF_LED_PIN in config:
        led_pin = await cg.gpio_pin_expression(config[CONF_LED_PIN])
        cg.add(var.set_led_pin(led_pin))
        cg.add(var.set_led_blink_time(config[CONF_LED_BLINK_TIME].total_milliseconds))

    cg.add_library("SPI", None)
    cg.add_library("LSatan/SmartRC-CC1101-Driver-Lib", "2.5.7")

    cg.add_platformio_option("build_src_filter", ["+<*>", "-<.git/>", "-<.svn/>"])

    if config[CONF_ALL_DRIVERS]:
        cg.add_platformio_option("build_src_filter", ["+<**/wmbus/driver_*.cpp>"])
    else:
        cg.add_platformio_option("build_src_filter", ["-<**/wmbus/driver_*.cpp>"])

    cg.add_platformio_option("build_src_filter", ["+<**/wmbus/driver_unknown.cpp>"])
