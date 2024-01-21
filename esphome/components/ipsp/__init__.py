import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)
from esphome.components.nrf52 import add_zephyr_prj_conf_option

ipsp_ns = cg.esphome_ns.namespace("ipsp")
Network = ipsp_ns.class_("Network", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Network),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_zephyr,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    add_zephyr_prj_conf_option("CONFIG_BT", True)
    add_zephyr_prj_conf_option("CONFIG_BT_SMP", True)
    add_zephyr_prj_conf_option("CONFIG_BT_PERIPHERAL", True)
    add_zephyr_prj_conf_option("CONFIG_BT_CENTRAL", True)
    add_zephyr_prj_conf_option("CONFIG_BT_L2CAP_DYNAMIC_CHANNEL", True)
    add_zephyr_prj_conf_option("CONFIG_BT_DEVICE_NAME", "Test IPSP node")  # TODO
    add_zephyr_prj_conf_option("CONFIG_NETWORKING", True)
    add_zephyr_prj_conf_option("CONFIG_NET_IPV6", True)
    add_zephyr_prj_conf_option("CONFIG_NET_IPV4", False)
    add_zephyr_prj_conf_option("CONFIG_NET_UDP", False)
    add_zephyr_prj_conf_option("CONFIG_NET_TCP", True)
    add_zephyr_prj_conf_option("CONFIG_TEST_RANDOM_GENERATOR", True)
    add_zephyr_prj_conf_option("CONFIG_NET_L2_BT", True)
    add_zephyr_prj_conf_option("CONFIG_INIT_STACKS", True)
    add_zephyr_prj_conf_option("CONFIG_NET_PKT_RX_COUNT", 10)
    add_zephyr_prj_conf_option("CONFIG_NET_PKT_TX_COUNT", 10)
    add_zephyr_prj_conf_option("CONFIG_NET_BUF_RX_COUNT", 20)
    add_zephyr_prj_conf_option("CONFIG_NET_BUF_TX_COUNT", 20)
    add_zephyr_prj_conf_option("CONFIG_NET_IF_UNICAST_IPV6_ADDR_COUNT", 3)
    add_zephyr_prj_conf_option("CONFIG_NET_IF_MCAST_IPV6_ADDR_COUNT", 4)
    add_zephyr_prj_conf_option("CONFIG_NET_MAX_CONTEXTS", 6)

    add_zephyr_prj_conf_option("CONFIG_NET_CONFIG_AUTO_INIT", True)
    add_zephyr_prj_conf_option("CONFIG_NET_CONFIG_SETTINGS", True)
    add_zephyr_prj_conf_option("CONFIG_NET_CONFIG_MY_IPV6_ADDR", "2001:db8::1")
    add_zephyr_prj_conf_option("CONFIG_NET_CONFIG_PEER_IPV6_ADDR", "2001:db8::2")
    add_zephyr_prj_conf_option("CONFIG_NET_CONFIG_BT_NODE", True)
    if True:
        add_zephyr_prj_conf_option("CONFIG_LOG", True)
        add_zephyr_prj_conf_option("CONFIG_NET_LOG", True)
