import esphome.codegen as cg
from esphome.components.light import compute_gamma_table
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Light gamma LUT benchmarks need USE_LIGHT_GAMMA_LUT defined
    # and a gamma table with external linkage that the benchmark can reference.
    manifest.enable_codegen()
    original_to_code = manifest.to_code

    async def to_code(config):
        await original_to_code(config)
        cg.add_define("USE_LIGHT_GAMMA_LUT")
        # Generate the gamma 2.8 table using the light component's own
        # compute_gamma_table() so the benchmark stays in sync with any
        # formula changes (e.g. the min-of-1 clamp in #15060).
        forward = compute_gamma_table(2.8)
        values = ", ".join(f"0x{int(v):04X}" for v in forward)
        # Use extern-visible (non-static) array so the benchmark .cpp
        # can reference it. cg.progmem_array() emits static constexpr
        # which has internal linkage and can't be extern'd.
        # In C++, const at namespace scope has internal linkage by default.
        # Use extern to give the array external linkage so the benchmark
        # translation unit can reference it.
        cg.add_global(
            cg.RawStatement(
                f"extern const uint16_t bench_gamma_2_8_fwd[256] PROGMEM = {{{values}}};"
            )
        )

    to_code.priority = original_to_code.priority
    manifest.to_code = to_code
