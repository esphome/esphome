from esphome.components import gauge_needle_detector
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The component has no YAML configuration, so the tests enable every
    # detector through the same helper a consumer uses.
    async def to_code_testing(config: ConfigType) -> None:
        gauge_needle_detector.enable_detectors({"auto"})

    manifest.to_code = to_code_testing
