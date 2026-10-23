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
    # One empty config entry so to_code runs.
    manifest.multi_conf = False

    async def to_code(config: ConfigType) -> None:
        # No entity platforms are configured, so count them to emit USE_SENSOR,
        # USE_BINARY_SENSOR and USE_TEXT_SENSOR for the entity code under test.
        CORE.platform_counts["sensor"] += 1
        CORE.platform_counts["binary_sensor"] += 1
        CORE.platform_counts["text_sensor"] += 1

    manifest.to_code = to_code
