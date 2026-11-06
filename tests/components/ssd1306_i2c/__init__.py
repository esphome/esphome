"""Build the planner and driver with real display, I2C and core sources."""

import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    """Include the driver's dependencies without constructing hardware devices."""
    manifest.dependencies = ["display", "i2c", "ssd1306_base"]

    async def to_code(_config: ConfigType) -> None:
        cg.add_define("USE_SSD1306_I2C_PARTIAL_UPDATES")

    manifest.to_code = to_code
