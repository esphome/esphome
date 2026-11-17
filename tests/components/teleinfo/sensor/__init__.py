from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The platform's own to_code needs a configured hub; the sensor class is built directly.
    async def to_code_testing(config: ConfigType) -> None:
        pass

    manifest.to_code = to_code_testing
