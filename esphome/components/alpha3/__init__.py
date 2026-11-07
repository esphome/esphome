import esphome.codegen as cg
from esphome.components import ble_client

CODEOWNERS = ["@jan-hofmeier"]
alpha3_ns = cg.esphome_ns.namespace("alpha3")
Alpha3 = alpha3_ns.class_("Alpha3", ble_client.BLEClientNode, cg.PollingComponent)
