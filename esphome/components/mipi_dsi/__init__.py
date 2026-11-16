from dataclasses import dataclass, field
from typing import Any

import esphome.codegen as cg
from esphome.core import CORE, ID

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "mipi_dsi"

mipi_dsi_ns = cg.esphome_ns.namespace("mipi_dsi")


@dataclass
class MipiDsiData:
    framebuffer_readback_ids: list[ID] = field(default_factory=list)
    rgb565_transform_ids: list[ID] = field(default_factory=list)

    def get(self, name: str, default: Any = None) -> Any:
        """Expose capability lists without creating a second mapping owner."""
        return getattr(self, name, default)


def get_data() -> MipiDsiData:
    return CORE.data.setdefault(DOMAIN, MipiDsiData())
