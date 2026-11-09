import esphome.codegen as cg
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The harness walks dependencies from the component manifest, not from the
    # climate platform, so declare the platform's auto-load here. Otherwise the
    # transmitter's code generation runs and the host build cannot link it.
    manifest.auto_load = ["climate_ir", "climate", "select"]

    async def to_code_testing(config: ConfigType) -> None:
        # on_receive decodes the generic frame through the shared protocol class
        cg.add_define("USE_REMOTE_PROTOCOL_TOSHIBA_AC")
        cg.add_define("USE_TOSHIBA_RAS_25J2KVSG_ND")
        CORE.platform_counts["select"] += 1

    manifest.to_code = to_code_testing
