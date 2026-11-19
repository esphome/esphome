from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a udp packet_interface, whose to_code waits on the udp component variable.
    manifest.enable_codegen()
