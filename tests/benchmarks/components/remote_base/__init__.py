import esphome.codegen as cg
from esphome.core import CORE, ID
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

# Listener slots for the binary sensor fan-out benchmark
LISTENER_SLOTS = 8


def override_manifest(manifest: ComponentManifestOverride) -> None:
    from esphome.components import remote_base

    async def to_code(config: ConfigType) -> None:
        # Build every in-tree dumper, as `dump: all` does, into a table the benchmarks read
        dumpers = []
        for name, entry in remote_base.DUMPER_REGISTRY.items():
            if remote_base._protocol_stem(name) not in remote_base._PROTOCOL_STEMS:
                continue
            dumper_id = ID(
                f"bench_dump_{name}", is_declaration=True, type=entry.type_id
            )
            dumpers.append(await entry.coroutine_fun(entry.schema({}), dumper_id))
        table = ", ".join(str(d) for d in dumpers)
        CORE.add_global_after_includes(
            cg.RawStatement(
                "namespace esphome::benchmarks {\n"
                f"extern remote_base::RemoteReceiverDumperBase *const DUMP_ALL[] = {{{table}}};\n"
                f"extern const size_t DUMP_ALL_COUNT = {len(dumpers)};\n"
                "}"
            )
        )
        # NEC, RCSwitch and Coolix are already requested by their dumpers
        cg.add_define("REMOTE_BASE_DUMPER_COUNT", 1)
        cg.add_define("REMOTE_BASE_LISTENER_COUNT", LISTENER_SLOTS)

    manifest.to_code = to_code
    # AUTO_LOAD is not resolved by the benchmark build.
    manifest.dependencies = manifest.dependencies + ["binary_sensor"]
