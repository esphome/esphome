import esphome.codegen as cg
from esphome.components import ethernet, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DNS_ADDRESS,
    CONF_IP_ADDRESS,
    CONF_MAC_ADDRESS,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["ethernet"]

ethernet_info_ns = cg.esphome_ns.namespace("ethernet_info")

IPAddressEthernetInfo = ethernet_info_ns.class_(
    "IPAddressEthernetInfo", text_sensor.TextSensor, cg.Component
)

DNSAddressEthernetInfo = ethernet_info_ns.class_(
    "DNSAddressEthernetInfo", text_sensor.TextSensor, cg.Component
)

MACAddressEthernetInfo = ethernet_info_ns.class_(
    "MACAddressEthernetInfo", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IP_ADDRESS): text_sensor.text_sensor_schema(
            IPAddressEthernetInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(
            {
                cv.Optional(f"address_{x}"): text_sensor.text_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                )
                for x in range(5)
            }
        ),
        cv.Optional(CONF_DNS_ADDRESS): text_sensor.text_sensor_schema(
            DNSAddressEthernetInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
            MACAddressEthernetInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)


async def to_code(config):
    # Request Ethernet IP state listener slots - one per sensor type
    if CONF_IP_ADDRESS in config:
        ethernet.request_ethernet_ip_state_listener()
    if CONF_DNS_ADDRESS in config:
        ethernet.request_ethernet_ip_state_listener()

    if conf := config.get(CONF_IP_ADDRESS):
        ip_info = await text_sensor.new_text_sensor(config[CONF_IP_ADDRESS])
        await cg.register_component(ip_info, config[CONF_IP_ADDRESS])
        for x in range(5):
            if sensor_conf := conf.get(f"address_{x}"):
                sens = await text_sensor.new_text_sensor(sensor_conf)
                cg.add(ip_info.add_ip_sensors(x, sens))
    if conf := config.get(CONF_DNS_ADDRESS):
        dns_info = await text_sensor.new_text_sensor(conf)
        await cg.register_component(dns_info, conf)
    if conf := config.get(CONF_MAC_ADDRESS):
        mac_info = await text_sensor.new_text_sensor(conf)
        await cg.register_component(mac_info, conf)
