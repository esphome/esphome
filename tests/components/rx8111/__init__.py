"""Load the RX8111 time platform's dependencies in the C++ test build."""

import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = manifest.dependencies + [
        "i2c",
        "time",
        "text_sensor",
        "binary_sensor",
    ]

    async def to_code(config: ConfigType) -> None:
        cg.add_define("USE_TEXT_SENSOR")
        cg.add_define("USE_RX8111_TIMESTAMP")
        cg.add_define("ESPHOME_ENTITY_TEXT_SENSOR_COUNT", 0)
        cg.add_define("USE_BINARY_SENSOR")
        cg.add_define("ESPHOME_ENTITY_BINARY_SENSOR_COUNT", 0)

    manifest.to_code = to_code
