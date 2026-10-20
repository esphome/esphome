import esphome.codegen as cg
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The hub's to_code emits no feature defines; this platform stub turns on the parameter and
    # version code in the real ld2410s.cpp for the host test build
    async def to_code_testing(config: ConfigType) -> None:
        CORE.testing_ensure_platform_registered("text_sensor")
        cg.add_define("USE_LD2410S_PARAMETERS")
        cg.add_define("USE_LD2410S_VERSION")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["text_sensor"]
