import esphome.codegen as cg
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        for platform in ("sensor", "binary_sensor", "number", "text_sensor"):
            CORE.testing_ensure_platform_registered(platform)
        cg.add_define("USE_TMP102_CONFIGURE")
        cg.add_define("USE_TMP102_BINARY_SENSOR")
        cg.add_define("USE_TMP102_NUMBER")
        cg.add_define("USE_TMP102_TEXT_SENSOR")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + [
        "i2c",
        "sensor",
        "binary_sensor",
        "number",
        "text_sensor",
    ]
