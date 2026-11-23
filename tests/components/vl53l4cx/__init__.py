import esphome.codegen as cg
from esphome.components.vl53l4cx.sensor import VL53L4CX_VERSION
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The driver lives in the sensor platform, so pull in what it builds against, ST's library included.
    manifest.dependencies = manifest.dependencies + ["i2c", "sensor"]

    async def to_code(config: ConfigType) -> None:
        cg.add_library("esphome/VL53L4CX", VL53L4CX_VERSION)

    manifest.to_code = to_code
