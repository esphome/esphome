import esphome.codegen as cg
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        for platform in ("sensor", "number", "binary_sensor", "text_sensor"):
            CORE.testing_ensure_platform_registered(platform)
        for platform in ("NUMBER", "BINARY_SENSOR", "TEXT_SENSOR"):
            cg.add_define(f"USE_TMP102_{platform}")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + [
        "i2c",
        "sensor",
        "number",
        "binary_sensor",
        "text_sensor",
    ]
