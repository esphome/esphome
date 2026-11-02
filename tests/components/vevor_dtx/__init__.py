from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # The gtests attach every entity kind the hub can publish to
        for platform in ("sensor", "binary_sensor", "text_sensor"):
            CORE.testing_ensure_platform_registered(platform)

    manifest.to_code = to_code_testing
    # The frames come from the test, not from a receiver on a GPIO
    manifest.dependencies = ["remote_base", "sensor", "binary_sensor", "text_sensor"]
    # The host test config holds one plain hub entry; a multi-instance component would get an empty list
    manifest.multi_conf = False
