from pathlib import Path
from types import SimpleNamespace

import pytest

boto3 = pytest.importorskip("boto3")
botocore_unsigned = pytest.importorskip("botocore", reason="botocore is required for S3 mocking")
UNSIGNED = botocore_unsigned.UNSIGNED
moto_server = pytest.importorskip("moto.server", reason="moto is required for S3 integration testing")
ThreadedMotoServer = moto_server.ThreadedMotoServer

from script.compile_backend_service import _derive_output_name, _upload_factory_image, process_configuration


def test_derive_output_name_trims_trailing_one():
    assert _derive_output_name("xyz1111") == "xyz111.bin"
    assert _derive_output_name("device") == "device.bin"


def test_upload_factory_image_supports_unsigned_path_style(tmp_path, monkeypatch):
    upload_calls = []
    session_kwargs_captured = {}
    client_kwargs_captured = {}

    image_path = tmp_path / "firmware.factory.bin"
    image_path.write_bytes(b"binary")

    class FakeClient:
        def __init__(self, kwargs: dict):
            client_kwargs_captured.update(kwargs)

        def upload_file(self, filename: str, bucket: str, key: str) -> None:
            upload_calls.append((filename, bucket, key))

    class FakeSession:
        def __init__(self, **kwargs):
            session_kwargs_captured.update(kwargs)

        def client(self, name: str, **kwargs):
            assert name == "s3"
            return FakeClient(kwargs)

    monkeypatch.setattr(boto3, "session", SimpleNamespace(Session=FakeSession))

    _upload_factory_image(
        image_path,
        bucket="mock-bucket",
        key="path/to/file.bin",
        region="us-east-1",
        endpoint="https://mock-s3.local",
        path_style=True,
        unsigned=True,
    )

    assert session_kwargs_captured == {"region_name": "us-east-1"}
    assert client_kwargs_captured["endpoint_url"] == "https://mock-s3.local"
    config = client_kwargs_captured["config"]
    assert config.s3 == {"addressing_style": "path"}
    assert config.signature_version is UNSIGNED
    assert upload_calls == [(str(image_path), "mock-bucket", "path/to/file.bin")]


YAML_CONTENT = """\
substitutions:
  device_name: "xyz1111"
  friendly_name: "Integration Test"

esphome:
  name: ${device_name}

esp32:
  board: esp32dev

logger:

wifi:
  ssid: "ssid"
  password: "password"
"""


@pytest.fixture()
def moto_s3_server():
    server = ThreadedMotoServer()
    server.start()
    try:
        yield f"http://{server.address}:{server.port}"
    finally:
        server.stop()


def test_process_configuration_end_to_end(monkeypatch, tmp_path, moto_s3_server):
    endpoint = moto_s3_server
    bucket = "compile-bucket"
    client = boto3.client("s3", endpoint_url=endpoint, region_name="us-east-1")
    client.create_bucket(Bucket=bucket)

    yaml_key = "configs/xyz1111.yaml"
    client.put_object(Bucket=bucket, Key=yaml_key, Body=YAML_CONTENT)

    config_url = f"{endpoint}/{bucket}/{yaml_key}"
    callbacks = []

    def fake_post_json(url: str | None, payload: dict) -> None:
        callbacks.append((url, payload))

    def fake_run_cli(command, workdir):
        yaml_path = Path(command[-1])
        build_dir = yaml_path.parent / "build" / yaml_path.stem
        build_dir.mkdir(parents=True, exist_ok=True)
        if "compile" in command:
            (build_dir / "firmware.factory.bin").write_bytes(b"compiled-binary")
            return 0, "compile ok"
        return 0, "config ok"

    monkeypatch.setattr("script.compile_backend_service._post_json", fake_post_json)
    monkeypatch.setattr("script.compile_backend_service._run_cli", fake_run_cli)

    args = SimpleNamespace(
        config_url=config_url,
        upload_bucket=bucket,
        upload_prefix="",
        status_callback="https://status.local/callback",
        completion_callback="https://complete.local/callback",
        workspace=tmp_path,
        aws_region="us-east-1",
        s3_endpoint=endpoint,
        s3_path_style=True,
        s3_unsigned=False,
        log_level="DEBUG",
    )

    process_configuration(args)

    uploaded = client.get_object(Bucket=bucket, Key="xyz111.bin")
    assert uploaded["Body"].read() == b"compiled-binary"

    assert callbacks[0][1]["stage"] == "validation"
    assert callbacks[0][1]["success"] is True
    assert callbacks[1][1]["stage"] == "compile"
    assert callbacks[1][1]["success"] is True
    assert callbacks[2][1]["stage"] == "complete"
    assert callbacks[2][1]["success"] is True
    assert callbacks[2][1]["uploaded_key"] == "xyz111.bin"
