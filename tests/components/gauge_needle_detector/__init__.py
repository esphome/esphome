import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The component has no YAML configuration, so its sources compile to nothing
    # unless the per-algorithm defines are set. Enable all detectors for the tests.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_GAUGE_NEEDLE_DETECTOR")
        cg.add_define("USE_GAUGE_NEEDLE_INTENSITY_SCAN")
        cg.add_define("USE_GAUGE_NEEDLE_RADIAL_PROFILE")
        cg.add_define("USE_GAUGE_NEEDLE_HOUGH_TRANSFORM")
        cg.add_define("USE_GAUGE_NEEDLE_TEMPLATE_MATCH")

    manifest.to_code = to_code_testing
