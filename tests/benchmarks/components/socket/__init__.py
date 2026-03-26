from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # socket must run its to_code to define USE_SOCKET_IMPL_BSD_SOCKETS
    # which is needed by the api frame helper benchmarks.
    manifest.enable_codegen()
