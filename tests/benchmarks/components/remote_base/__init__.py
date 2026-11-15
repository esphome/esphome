import esphome.codegen as cg
from esphome.components import remote_base
from esphome.core import CORE, ID
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code(config: ConfigType) -> None:
        # Called directly: validate_dumpers plus build_dumpers would leave the GenerateIDs unresolved
        dumpers = [
            await entry.coroutine_fun(
                entry.schema({}),
                ID(f"bench_dump_{name}", is_declaration=True, type=entry.type_id),
            )
            for name, entry in remote_base.DUMPER_REGISTRY.items()
        ]
        CORE.add_global_after_includes(
            cg.RawStatement(
                "namespace esphome::benchmarks {\n"
                f"extern remote_base::RemoteReceiverDumperBase *const DUMP_ALL[] = {cg.ArrayInitializer(*dumpers)};\n"
                f"extern const size_t DUMP_ALL_COUNT = {len(dumpers)};\n"
                "}"
            )
        )
        # Set directly, not through add_dumper/add_listener: the C++ registers its dumper table and listeners itself
        cg.add_define("REMOTE_BASE_DUMPER_COUNT", 1)
        cg.add_define("REMOTE_BASE_LISTENER_COUNT", 8)

    manifest.to_code = to_code
