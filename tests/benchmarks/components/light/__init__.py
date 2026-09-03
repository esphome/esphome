import esphome.codegen as cg
from esphome.components.light import generate_gamma_table
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Light benchmarks need USE_LIGHT_GAMMA_LUT defined and a gamma table
    # with external linkage that the benchmark .cpp can reference.
    manifest.enable_codegen()
    original_to_code = manifest.to_code

    async def to_code(config):
        await original_to_code(config)
        cg.add_define("USE_LIGHT_GAMMA_LUT")
        # Use the light component's own generate_gamma_table() so the
        # benchmark stays in sync with any formula changes.
        forward = generate_gamma_table(2.8)
        values = ", ".join(f"0x{int(v):04X}" for v in forward)
        # Use extern-visible (non-static) array so the benchmark .cpp
        # can reference it via extern declaration.
        cg.add_global(
            cg.RawStatement(
                f"extern const uint16_t bench_gamma_2_8_fwd[256] PROGMEM = {{{values}}};"
            )
        )

    to_code.priority = original_to_code.priority
    manifest.to_code = to_code
