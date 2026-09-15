CODEOWNERS = ["@ti-mo"]
import esphome.codegen as cg
from esphome.components import cover

tormatic_ns = cg.esphome_ns.namespace("tormatic")

Tormatic = tormatic_ns.class_(
    "Tormatic",
    cover.Cover,
    cg.PollingComponent,
)
