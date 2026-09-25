import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # Keep access to calibration state confined to unit-test builds.
        cg.add_build_flag("-fno-access-control")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["sensor", "spi"]
