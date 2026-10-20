"""Build the pure planner independently of display and I2C hardware."""

import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

PLANNER_SOURCES = ("ssd1306_partial_update.cpp", "ssd1306_partial_update.h")


def override_manifest(manifest: ComponentManifestOverride) -> None:
    """Select the production planner sources for native C++ unit tests."""
    manifest.resources = [
        resource
        for resource in manifest.resources
        if resource.resource in PLANNER_SOURCES
    ]

    async def to_code(_config: ConfigType) -> None:
        cg.add_define("USE_SSD1306_I2C_PARTIAL_UPDATES")

    manifest.to_code = to_code
