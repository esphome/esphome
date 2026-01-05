import esphome.codegen as cg
from esphome.components import ble_client

CODEOWNERS = ["@jan-hofmeier"]

alpha3_ns = cg.esphome_ns.namespace("alpha3")
Alpha3 = alpha3_ns.class_("Alpha3", ble_client.BLEClientNode, cg.PollingComponent)

CONF_ALPHA3_ID = "alpha3_id"

__all__ = ["alpha3_ns", "Alpha3", "CONF_ALPHA3_ID"]
