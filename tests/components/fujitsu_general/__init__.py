from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # This component's AUTO_LOAD = ["climate_ir"] sits on the climate platform manifest, while its
    # own __init__.py is empty. The unit test build resolves the bare `fujitsu_general` domain, so
    # it never sees that manifest. And climate_ir itself doesn't declare `climate` even though
    # ClimateIR derives from climate::Climate. Pull both in so the test can include the header.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate"]
