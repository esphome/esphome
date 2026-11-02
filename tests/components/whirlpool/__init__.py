import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The harness walks dependencies from the component manifest, not from the
    # climate platform, so declare the platform's auto-load here. Otherwise the
    # transmitter's code generation runs and the host build cannot link it.
    manifest.auto_load = ["climate_ir", "climate"]

    async def to_code_testing(config: ConfigType) -> None:
        # Compiles in the Hisense ARG model, which climate.py adds only when it is configured
        cg.add_define("USE_WHIRLPOOL_HISENSE_ARG")

    manifest.to_code = to_code_testing
