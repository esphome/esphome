"""The reader's math is header-only; the device code is compiled out on the host."""

from esphome.components import gauge_needle_detector
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    """Load only the components whose headers the math header includes."""

    async def to_code_testing(config: ConfigType) -> None:
        gauge_needle_detector.enable_detectors({"auto"})

    manifest.dependencies = ["camera", "gauge_needle_detector", "sensor"]
    manifest.to_code = to_code_testing
