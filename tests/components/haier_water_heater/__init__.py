from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Include the real entity classes used by the controller fixtures.
    manifest.auto_load = [
        "uart",
        "water_heater",
        "sensor",
        "binary_sensor",
        "text_sensor",
    ]

    async def to_code_testing(config: ConfigType) -> None:
        for platform in ("sensor", "binary_sensor", "text_sensor"):
            CORE.testing_ensure_platform_registered(platform)

    manifest.to_code = to_code_testing
