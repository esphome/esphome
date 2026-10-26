import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    from esphome.components.remote_base import request_protocol

    async def to_code_testing(config: ConfigType) -> None:
        # Protocol sources are compiled only behind their define; keep the ones under test.
        for protocol in (
            "hob2hood",
            "haier",
            "mirage",
            "aeha",
            "aok",
            "keeloq",
            "nexus",
            "ira211",
            "nec",
            "magiquest",
            "rc5x",
            "pioneer_wyt",
            "drayton",
            "panasonic",
            "cardin_s438",
        ):
            request_protocol(protocol)
        # receiver_dumpers_test.cpp registers dumpers by hand
        cg.add_define("REMOTE_BASE_DUMPER_COUNT", 4)

    manifest.to_code = to_code_testing
    # AUTO_LOAD is not resolved by the unit test build.
    manifest.dependencies = manifest.dependencies + ["binary_sensor"]
