import logging
import os
from pathlib import Path
import socket
import stat
from unittest.mock import patch

from aioesphomeapi.host_resolver import AddrInfo, IPv4Sockaddr, IPv6Sockaddr
from hypothesis import given
from hypothesis.strategies import ip_addresses
import pytest

from esphome import helpers
from esphome.address_cache import AddressCache
from esphome.core import EsphomeError


@pytest.mark.parametrize(
    "preferred_string, current_strings, expected",
    (
        ("foo", [], "foo"),
        # TODO: Should this actually start at 1?
        ("foo", ["foo"], "foo_2"),
        ("foo", ("foo",), "foo_2"),
        ("foo", ("foo", "foo_2"), "foo_3"),
        ("foo", ("foo", "foo_2", "foo_2"), "foo_3"),
    ),
)
def test_ensure_unique_string(preferred_string, current_strings, expected):
    actual = helpers.ensure_unique_string(preferred_string, current_strings)

    assert actual == expected


@pytest.mark.parametrize(
    "text, expected",
    (
        ("foo", "foo"),
        ("foo\nbar", "foo\nbar"),
        ("foo\nbar\neek", "foo\n  bar\neek"),
    ),
)
def test_indent_all_but_first_and_last(text, expected):
    actual = helpers.indent_all_but_first_and_last(text)

    assert actual == expected


@pytest.mark.parametrize(
    "text, expected",
    (
        ("foo", ["  foo"]),
        ("foo\nbar", ["  foo", "  bar"]),
        ("foo\nbar\neek", ["  foo", "  bar", "  eek"]),
    ),
)
def test_indent_list(text, expected):
    actual = helpers.indent_list(text)

    assert actual == expected


@pytest.mark.parametrize(
    "text, expected",
    (
        ("foo", "  foo"),
        ("foo\nbar", "  foo\n  bar"),
        ("foo\nbar\neek", "  foo\n  bar\n  eek"),
    ),
)
def test_indent(text, expected):
    actual = helpers.indent(text)

    assert actual == expected


@pytest.mark.parametrize(
    "string, expected",
    (
        ("foo", '"foo"'),
        ("foo\nbar", '"foo\\012bar"'),
        ("foo\\bar", '"foo\\134bar"'),
        ('foo "bar"', '"foo \\042bar\\042"'),
        ("foo 🐍", '"foo \\360\\237\\220\\215"'),
    ),
)
def test_cpp_string_escape(string, expected):
    actual = helpers.cpp_string_escape(string)

    assert actual == expected


@pytest.mark.parametrize(
    "host",
    (
        "127.0.0",
        "localhost",
        "127.0.0.b",
    ),
)
def test_is_ip_address__invalid(host):
    actual = helpers.is_ip_address(host)

    assert actual is False


@given(value=ip_addresses(v=4).map(str))
def test_is_ip_address__valid(value):
    actual = helpers.is_ip_address(value)

    assert actual is True


@pytest.mark.parametrize(
    "var, value, default, expected",
    (
        ("FOO", None, False, False),
        ("FOO", None, True, True),
        ("FOO", "", False, False),
        ("FOO", "False", False, False),
        ("FOO", "True", False, True),
        ("FOO", "FALSE", True, False),
        ("FOO", "fAlSe", True, False),
        ("FOO", "Yes", False, True),
        ("FOO", "123", False, True),
    ),
)
def test_get_bool_env(monkeypatch, var, value, default, expected):
    if value is None:
        monkeypatch.delenv(var, raising=False)
    else:
        monkeypatch.setenv(var, value)

    actual = helpers.get_bool_env(var, default)

    assert actual == expected


@pytest.mark.parametrize("value, expected", ((None, False), ("Yes", True)))
def test_is_ha_addon(monkeypatch, value, expected):
    if value is None:
        monkeypatch.delenv("ESPHOME_IS_HA_ADDON", raising=False)
    else:
        monkeypatch.setenv("ESPHOME_IS_HA_ADDON", value)

    actual = helpers.is_ha_addon()

    assert actual == expected


def test_walk_files(fixture_path):
    path = fixture_path / "helpers"

    actual = list(helpers.walk_files(path))

    # Ensure paths start with the root
    assert all(p.is_relative_to(path) for p in actual)


class Test_write_file_if_changed:
    def test_src_and_dst_match(self, tmp_path: Path):
        text = "A files are unique.\n"
        initial = text
        dst = tmp_path / "file-a.txt"
        dst.write_text(initial)

        helpers.write_file_if_changed(dst, text)

        assert dst.read_text() == text

    def test_src_and_dst_do_not_match(self, tmp_path: Path):
        text = "A files are unique.\n"
        initial = "B files are unique.\n"
        dst = tmp_path / "file-a.txt"
        dst.write_text(initial)

        helpers.write_file_if_changed(dst, text)

        assert dst.read_text() == text

    def test_dst_does_not_exist(self, tmp_path: Path):
        text = "A files are unique.\n"
        dst = tmp_path / "file-a.txt"

        helpers.write_file_if_changed(dst, text)

        assert dst.read_text() == text


class Test_copy_file_if_changed:
    def test_src_and_dst_match(self, tmp_path: Path, fixture_path: Path):
        src = fixture_path / "helpers" / "file-a.txt"
        initial = fixture_path / "helpers" / "file-a.txt"
        dst = tmp_path / "file-a.txt"

        dst.write_text(initial.read_text())

        helpers.copy_file_if_changed(src, dst)

    def test_src_and_dst_do_not_match(self, tmp_path: Path, fixture_path: Path):
        src = fixture_path / "helpers" / "file-a.txt"
        initial = fixture_path / "helpers" / "file-c.txt"
        dst = tmp_path / "file-a.txt"

        dst.write_text(initial.read_text())

        helpers.copy_file_if_changed(src, dst)

        assert src.read_text() == dst.read_text()

    def test_dst_does_not_exist(self, tmp_path: Path, fixture_path: Path):
        src = fixture_path / "helpers" / "file-a.txt"
        dst = tmp_path / "file-a.txt"

        helpers.copy_file_if_changed(src, dst)

        assert dst.exists()
        assert src.read_text() == dst.read_text()


@pytest.mark.parametrize(
    "file1, file2, expected",
    (
        # Same file
        ("file-a.txt", "file-a.txt", True),
        # Different files, different size
        ("file-a.txt", "file-b_1.txt", False),
        # Different files, same size
        ("file-a.txt", "file-c.txt", False),
        # Same files
        ("file-b_1.txt", "file-b_2.txt", True),
        # Not a file
        ("file-a.txt", "", False),
        # File doesn't exist
        ("file-a.txt", "file-d.txt", False),
    ),
)
def test_file_compare(fixture_path, file1, file2, expected):
    path1 = fixture_path / "helpers" / file1
    path2 = fixture_path / "helpers" / file2

    actual = helpers.file_compare(path1, path2)

    assert actual == expected


@pytest.mark.parametrize(
    "text, expected",
    (
        ("foo", "foo"),
        ("foo bar", "foo_bar"),
        ("foo Bar", "foo_bar"),
        ("foo BAR", "foo_bar"),
        ("foo.bar", "foo.bar"),
        ("fooBAR", "foobar"),
        ("Foo-bar_EEK", "foo-bar_eek"),
        ("  foo", "__foo"),
    ),
)
def test_snake_case(text, expected):
    actual = helpers.snake_case(text)

    assert actual == expected


@pytest.mark.parametrize(
    "text, expected",
    (
        ("foo_bar", "foo_bar"),
        ('!"§$%&/()=?foo_bar', "___________foo_bar"),
        ('foo_!"§$%&/()=?bar', "foo____________bar"),
        ('foo_bar!"§$%&/()=?', "foo_bar___________"),
        ('foo-bar!"§$%&/()=?', "foo-bar___________"),
    ),
)
def test_sanitize(text, expected):
    actual = helpers.sanitize(text)

    assert actual == expected


@pytest.mark.parametrize(
    "text, expected",
    ((["127.0.0.1", "fe80::1", "2001::2"], ["2001::2", "127.0.0.1", "fe80::1"]),),
)
def test_sort_ip_addresses(text: list[str], expected: list[str]) -> None:
    actual = helpers.sort_ip_addresses(text)

    assert actual == expected


# DNS resolution tests
def test_is_ip_address_ipv4() -> None:
    """Test is_ip_address with IPv4 addresses."""
    assert helpers.is_ip_address("192.168.1.1") is True
    assert helpers.is_ip_address("127.0.0.1") is True
    assert helpers.is_ip_address("255.255.255.255") is True
    assert helpers.is_ip_address("0.0.0.0") is True


def test_is_ip_address_ipv6() -> None:
    """Test is_ip_address with IPv6 addresses."""
    assert helpers.is_ip_address("::1") is True
    assert helpers.is_ip_address("2001:db8::1") is True
    assert helpers.is_ip_address("fe80::1") is True
    assert helpers.is_ip_address("::") is True


def test_is_ip_address_invalid() -> None:
    """Test is_ip_address with non-IP strings."""
    assert helpers.is_ip_address("hostname") is False
    assert helpers.is_ip_address("hostname.local") is False
    assert helpers.is_ip_address("256.256.256.256") is False
    assert helpers.is_ip_address("192.168.1") is False
    assert helpers.is_ip_address("") is False


def test_resolve_ip_address_single_ipv4() -> None:
    """Test resolving a single IPv4 address (fast path)."""
    result = helpers.resolve_ip_address("192.168.1.100", 6053)

    assert len(result) == 1
    assert result[0][0] == socket.AF_INET  # family
    assert result[0][1] in (
        0,
        socket.SOCK_STREAM,
    )  # type (0 on Windows with AI_NUMERICHOST)
    assert result[0][2] in (
        0,
        socket.IPPROTO_TCP,
    )  # proto (0 on Windows with AI_NUMERICHOST)
    assert result[0][3] == ""  # canonname
    assert result[0][4] == ("192.168.1.100", 6053)  # sockaddr


def test_resolve_ip_address_single_ipv6() -> None:
    """Test resolving a single IPv6 address (fast path)."""
    result = helpers.resolve_ip_address("::1", 6053)

    assert len(result) == 1
    assert result[0][0] == socket.AF_INET6  # family
    assert result[0][1] in (
        0,
        socket.SOCK_STREAM,
    )  # type (0 on Windows with AI_NUMERICHOST)
    assert result[0][2] in (
        0,
        socket.IPPROTO_TCP,
    )  # proto (0 on Windows with AI_NUMERICHOST)
    assert result[0][3] == ""  # canonname
    # IPv6 sockaddr has 4 elements
    assert len(result[0][4]) == 4
    assert result[0][4][0] == "::1"  # address
    assert result[0][4][1] == 6053  # port


def test_resolve_ip_address_list_of_ips() -> None:
    """Test resolving a list of IP addresses (fast path)."""
    ips = ["192.168.1.100", "10.0.0.1", "::1"]
    result = helpers.resolve_ip_address(ips, 6053)

    # Should return results sorted by preference (IPv6 first, then IPv4)
    assert len(result) >= 2  # At least IPv4 addresses should work

    # Check that results are properly formatted
    for addr_info in result:
        assert addr_info[0] in (socket.AF_INET, socket.AF_INET6)
        assert addr_info[1] in (
            0,
            socket.SOCK_STREAM,
        )  # 0 on Windows with AI_NUMERICHOST
        assert addr_info[2] in (
            0,
            socket.IPPROTO_TCP,
        )  # 0 on Windows with AI_NUMERICHOST
        assert addr_info[3] == ""


def test_resolve_ip_address_with_getaddrinfo_failure(caplog) -> None:
    """Test that getaddrinfo OSError is handled gracefully in fast path."""
    with (
        caplog.at_level(logging.DEBUG),
        patch("socket.getaddrinfo") as mock_getaddrinfo,
    ):
        # First IP succeeds
        mock_getaddrinfo.side_effect = [
            [
                (
                    socket.AF_INET,
                    socket.SOCK_STREAM,
                    socket.IPPROTO_TCP,
                    "",
                    ("192.168.1.100", 6053),
                )
            ],
            OSError("Failed to resolve"),  # Second IP fails
        ]

        # Should continue despite one failure
        result = helpers.resolve_ip_address(["192.168.1.100", "192.168.1.101"], 6053)

        # Should have result from first IP only
        assert len(result) == 1
        assert result[0][4][0] == "192.168.1.100"

        # Verify both IPs were attempted
        assert mock_getaddrinfo.call_count == 2
        mock_getaddrinfo.assert_any_call(
            "192.168.1.100", 6053, proto=socket.IPPROTO_TCP, flags=socket.AI_NUMERICHOST
        )
        mock_getaddrinfo.assert_any_call(
            "192.168.1.101", 6053, proto=socket.IPPROTO_TCP, flags=socket.AI_NUMERICHOST
        )

        # Verify the debug log was called for the failed IP
        assert "Failed to parse IP address '192.168.1.101'" in caplog.text


def test_resolve_ip_address_hostname() -> None:
    """Test resolving a hostname (async resolver path)."""
    mock_addr_info = AddrInfo(
        family=socket.AF_INET,
        type=socket.SOCK_STREAM,
        proto=socket.IPPROTO_TCP,
        sockaddr=IPv4Sockaddr(address="192.168.1.100", port=6053),
    )

    with patch("esphome.resolver.AsyncResolver") as MockResolver:
        mock_resolver = MockResolver.return_value
        mock_resolver.resolve.return_value = [mock_addr_info]

        result = helpers.resolve_ip_address("test.local", 6053)

        assert len(result) == 1
        assert result[0][0] == socket.AF_INET
        assert result[0][4] == ("192.168.1.100", 6053)
        MockResolver.assert_called_once_with(["test.local"], 6053)
        mock_resolver.resolve.assert_called_once()


def test_resolve_ip_address_mixed_list() -> None:
    """Test resolving a mix of IPs and hostnames."""
    mock_addr_info = AddrInfo(
        family=socket.AF_INET,
        type=socket.SOCK_STREAM,
        proto=socket.IPPROTO_TCP,
        sockaddr=IPv4Sockaddr(address="192.168.1.200", port=6053),
    )

    with patch("esphome.resolver.AsyncResolver") as MockResolver:
        mock_resolver = MockResolver.return_value
        mock_resolver.resolve.return_value = [mock_addr_info]

        # Mix of IP and hostname - should use async resolver
        result = helpers.resolve_ip_address(["192.168.1.100", "test.local"], 6053)

        assert len(result) == 1
        assert result[0][4][0] == "192.168.1.200"
        MockResolver.assert_called_once_with(["192.168.1.100", "test.local"], 6053)
        mock_resolver.resolve.assert_called_once()


def test_resolve_ip_address_url() -> None:
    """Test extracting hostname from URL."""
    mock_addr_info = AddrInfo(
        family=socket.AF_INET,
        type=socket.SOCK_STREAM,
        proto=socket.IPPROTO_TCP,
        sockaddr=IPv4Sockaddr(address="192.168.1.100", port=6053),
    )

    with patch("esphome.resolver.AsyncResolver") as MockResolver:
        mock_resolver = MockResolver.return_value
        mock_resolver.resolve.return_value = [mock_addr_info]

        result = helpers.resolve_ip_address("http://test.local", 6053)

        assert len(result) == 1
        MockResolver.assert_called_once_with(["test.local"], 6053)
        mock_resolver.resolve.assert_called_once()


def test_resolve_ip_address_ipv6_conversion() -> None:
    """Test proper IPv6 address info conversion."""
    mock_addr_info = AddrInfo(
        family=socket.AF_INET6,
        type=socket.SOCK_STREAM,
        proto=socket.IPPROTO_TCP,
        sockaddr=IPv6Sockaddr(address="2001:db8::1", port=6053, flowinfo=1, scope_id=2),
    )

    with patch("esphome.resolver.AsyncResolver") as MockResolver:
        mock_resolver = MockResolver.return_value
        mock_resolver.resolve.return_value = [mock_addr_info]

        result = helpers.resolve_ip_address("test.local", 6053)

        assert len(result) == 1
        assert result[0][0] == socket.AF_INET6
        assert result[0][4] == ("2001:db8::1", 6053, 1, 2)


def test_resolve_ip_address_error_handling() -> None:
    """Test error handling from AsyncResolver."""
    with patch("esphome.resolver.AsyncResolver") as MockResolver:
        mock_resolver = MockResolver.return_value
        mock_resolver.resolve.side_effect = EsphomeError("Resolution failed")

        with pytest.raises(EsphomeError, match="Resolution failed"):
            helpers.resolve_ip_address("test.local", 6053)


def test_addr_preference_ipv4() -> None:
    """Test address preference for IPv4."""
    addr_info = (
        socket.AF_INET,
        socket.SOCK_STREAM,
        socket.IPPROTO_TCP,
        "",
        ("192.168.1.1", 6053),
    )
    assert helpers.addr_preference_(addr_info) == 2


def test_addr_preference_ipv6() -> None:
    """Test address preference for regular IPv6."""
    addr_info = (
        socket.AF_INET6,
        socket.SOCK_STREAM,
        socket.IPPROTO_TCP,
        "",
        ("2001:db8::1", 6053, 0, 0),
    )
    assert helpers.addr_preference_(addr_info) == 1


def test_addr_preference_ipv6_link_local_no_scope() -> None:
    """Test address preference for link-local IPv6 without scope."""
    addr_info = (
        socket.AF_INET6,
        socket.SOCK_STREAM,
        socket.IPPROTO_TCP,
        "",
        ("fe80::1", 6053, 0, 0),  # link-local with scope_id=0
    )
    assert helpers.addr_preference_(addr_info) == 3


def test_addr_preference_ipv6_link_local_with_scope() -> None:
    """Test address preference for link-local IPv6 with scope."""
    addr_info = (
        socket.AF_INET6,
        socket.SOCK_STREAM,
        socket.IPPROTO_TCP,
        "",
        ("fe80::1", 6053, 0, 2),  # link-local with scope_id=2
    )
    assert helpers.addr_preference_(addr_info) == 1  # Has scope, so it's usable


def test_mkdir_p(tmp_path: Path) -> None:
    """Test mkdir_p creates directories recursively."""
    # Test creating nested directories
    nested_path = tmp_path / "level1" / "level2" / "level3"
    helpers.mkdir_p(nested_path)
    assert nested_path.exists()
    assert nested_path.is_dir()

    # Test that mkdir_p is idempotent (doesn't fail if directory exists)
    helpers.mkdir_p(nested_path)
    assert nested_path.exists()

    # Test with empty path (should do nothing)
    helpers.mkdir_p("")

    # Test with existing directory
    existing_dir = tmp_path / "existing"
    existing_dir.mkdir()
    helpers.mkdir_p(existing_dir)
    assert existing_dir.exists()


def test_mkdir_p_file_exists_error(tmp_path: Path) -> None:
    """Test mkdir_p raises error when path is a file."""
    # Create a file
    file_path = tmp_path / "test_file.txt"
    file_path.write_text("test content")

    # Try to create directory with same name as existing file
    with pytest.raises(EsphomeError, match=r"Error creating directories"):
        helpers.mkdir_p(file_path)


def test_mkdir_p_with_existing_file_raises_error(tmp_path: Path) -> None:
    """Test mkdir_p raises error when trying to create dir over existing file."""
    # Create a file where we want to create a directory
    file_path = tmp_path / "existing_file"
    file_path.write_text("content")

    # Try to create a directory with a path that goes through the file
    dir_path = file_path / "subdir"

    with pytest.raises(EsphomeError, match=r"Error creating directories"):
        helpers.mkdir_p(dir_path)


def test_read_file(tmp_path: Path) -> None:
    """Test read_file reads file content correctly."""
    # Test reading regular file
    test_file = tmp_path / "test.txt"
    expected_content = "Test content\nLine 2\n"
    test_file.write_text(expected_content)

    content = helpers.read_file(test_file)
    assert content == expected_content

    # Test reading file with UTF-8 characters
    utf8_file = tmp_path / "utf8.txt"
    utf8_content = "Hello 世界 🌍"
    utf8_file.write_text(utf8_content, encoding="utf-8")

    content = helpers.read_file(utf8_file)
    assert content == utf8_content


def test_read_file_not_found() -> None:
    """Test read_file raises error for non-existent file."""
    with pytest.raises(EsphomeError, match=r"Error reading file"):
        helpers.read_file(Path("/nonexistent/file.txt"))


def test_read_file_unicode_decode_error(tmp_path: Path) -> None:
    """Test read_file raises error for invalid UTF-8."""
    test_file = tmp_path / "invalid.txt"
    # Write invalid UTF-8 bytes
    test_file.write_bytes(b"\xff\xfe")

    with pytest.raises(EsphomeError, match=r"Error reading file"):
        helpers.read_file(test_file)


@pytest.mark.skipif(os.name == "nt", reason="Unix-specific test")
def test_write_file_unix(tmp_path: Path) -> None:
    """Test write_file writes content correctly on Unix."""
    # Test writing string content
    test_file = tmp_path / "test.txt"
    content = "Test content\nLine 2"
    helpers.write_file(test_file, content)

    assert test_file.read_text() == content
    # Check file permissions
    assert oct(test_file.stat().st_mode)[-3:] == "644"

    # Test overwriting existing file
    new_content = "New content"
    helpers.write_file(test_file, new_content)
    assert test_file.read_text() == new_content

    # Test writing to nested directories (should create them)
    nested_file = tmp_path / "dir1" / "dir2" / "file.txt"
    helpers.write_file(nested_file, content)
    assert nested_file.read_text() == content


@pytest.mark.skipif(os.name != "nt", reason="Windows-specific test")
def test_write_file_windows(tmp_path: Path) -> None:
    """Test write_file writes content correctly on Windows."""
    # Test writing string content
    test_file = tmp_path / "test.txt"
    content = "Test content\nLine 2"
    helpers.write_file(test_file, content)

    assert test_file.read_text() == content
    # Windows doesn't have Unix-style 644 permissions

    # Test overwriting existing file
    new_content = "New content"
    helpers.write_file(test_file, new_content)
    assert test_file.read_text() == new_content

    # Test writing to nested directories (should create them)
    nested_file = tmp_path / "dir1" / "dir2" / "file.txt"
    helpers.write_file(nested_file, content)
    assert nested_file.read_text() == content


@pytest.mark.skipif(os.name == "nt", reason="Unix-specific permission test")
def test_write_file_to_non_writable_directory_unix(tmp_path: Path) -> None:
    """Test write_file raises error when directory is not writable on Unix."""
    # Create a directory and make it read-only
    read_only_dir = tmp_path / "readonly"
    read_only_dir.mkdir()
    test_file = read_only_dir / "test.txt"

    # Make directory read-only (no write permission)
    read_only_dir.chmod(0o555)

    try:
        with pytest.raises(EsphomeError, match=r"Could not write file"):
            helpers.write_file(test_file, "content")
    finally:
        # Restore write permissions for cleanup
        read_only_dir.chmod(0o755)


@pytest.mark.skipif(os.name != "nt", reason="Windows-specific test")
def test_write_file_to_non_writable_directory_windows(tmp_path: Path) -> None:
    """Test write_file error handling on Windows."""
    # Windows handles permissions differently - test a different error case
    # Try to write to a file path that contains an existing file as a directory component
    existing_file = tmp_path / "file.txt"
    existing_file.write_text("content")

    # Try to write to a path that treats the file as a directory
    invalid_path = existing_file / "subdir" / "test.txt"

    with pytest.raises(EsphomeError, match=r"Could not write file"):
        helpers.write_file(invalid_path, "content")


@pytest.mark.skipif(os.name == "nt", reason="Unix-specific permission test")
def test_write_file_with_permission_bits_unix(tmp_path: Path) -> None:
    """Test that write_file sets correct permissions on Unix."""
    test_file = tmp_path / "test.txt"
    helpers.write_file(test_file, "content")

    # Check that file has 644 permissions
    file_mode = test_file.stat().st_mode
    assert stat.S_IMODE(file_mode) == 0o644


@pytest.mark.skipif(os.name == "nt", reason="Unix-specific permission test")
def test_copy_file_if_changed_permission_recovery_unix(tmp_path: Path) -> None:
    """Test copy_file_if_changed handles permission errors correctly on Unix."""
    # Test with read-only destination file
    src = tmp_path / "source.txt"
    dst = tmp_path / "dest.txt"
    src.write_text("new content")
    dst.write_text("old content")
    dst.chmod(0o444)  # Make destination read-only

    try:
        # Should handle permission error by deleting and retrying
        helpers.copy_file_if_changed(src, dst)
        assert dst.read_text() == "new content"
    finally:
        # Restore write permissions for cleanup
        if dst.exists():
            dst.chmod(0o644)


def test_copy_file_if_changed_creates_directories(tmp_path: Path) -> None:
    """Test copy_file_if_changed creates missing directories."""
    src = tmp_path / "source.txt"
    dst = tmp_path / "subdir" / "nested" / "dest.txt"
    src.write_text("content")

    helpers.copy_file_if_changed(src, dst)
    assert dst.exists()
    assert dst.read_text() == "content"


def test_copy_file_if_changed_nonexistent_source(tmp_path: Path) -> None:
    """Test copy_file_if_changed with non-existent source."""
    src = tmp_path / "nonexistent.txt"
    dst = tmp_path / "dest.txt"

    with pytest.raises(EsphomeError, match=r"Error copying file"):
        helpers.copy_file_if_changed(src, dst)


def test_resolve_ip_address_sorting() -> None:
    """Test that results are sorted by preference."""
    # Create multiple address infos with different preferences
    mock_addr_infos = [
        AddrInfo(
            family=socket.AF_INET6,
            type=socket.SOCK_STREAM,
            proto=socket.IPPROTO_TCP,
            sockaddr=IPv6Sockaddr(
                address="fe80::1", port=6053, flowinfo=0, scope_id=0
            ),  # Preference 3 (link-local no scope)
        ),
        AddrInfo(
            family=socket.AF_INET,
            type=socket.SOCK_STREAM,
            proto=socket.IPPROTO_TCP,
            sockaddr=IPv4Sockaddr(
                address="192.168.1.100", port=6053
            ),  # Preference 2 (IPv4)
        ),
        AddrInfo(
            family=socket.AF_INET6,
            type=socket.SOCK_STREAM,
            proto=socket.IPPROTO_TCP,
            sockaddr=IPv6Sockaddr(
                address="2001:db8::1", port=6053, flowinfo=0, scope_id=0
            ),  # Preference 1 (IPv6)
        ),
    ]

    with patch("esphome.resolver.AsyncResolver") as MockResolver:
        mock_resolver = MockResolver.return_value
        mock_resolver.resolve.return_value = mock_addr_infos

        result = helpers.resolve_ip_address("test.local", 6053)

        # Should be sorted: IPv6 first, then IPv4, then link-local without scope
        assert result[0][4][0] == "2001:db8::1"  # IPv6 (preference 1)
        assert result[1][4][0] == "192.168.1.100"  # IPv4 (preference 2)
        assert result[2][4][0] == "fe80::1"  # Link-local no scope (preference 3)


def test_resolve_ip_address_with_cache() -> None:
    """Test that the cache is used when provided."""
    cache = AddressCache(
        mdns_cache={"test.local": ["192.168.1.100", "192.168.1.101"]},
        dns_cache={
            "example.com": ["93.184.216.34", "2606:2800:220:1:248:1893:25c8:1946"]
        },
    )

    # Test mDNS cache hit
    result = helpers.resolve_ip_address("test.local", 6053, address_cache=cache)

    # Should return cached addresses without calling resolver
    assert len(result) == 2
    assert result[0][4][0] == "192.168.1.100"
    assert result[1][4][0] == "192.168.1.101"

    # Test DNS cache hit
    result = helpers.resolve_ip_address("example.com", 6053, address_cache=cache)

    # Should return cached addresses with IPv6 first due to preference
    assert len(result) == 2
    assert result[0][4][0] == "2606:2800:220:1:248:1893:25c8:1946"  # IPv6 first
    assert result[1][4][0] == "93.184.216.34"  # IPv4 second


def test_resolve_ip_address_cache_miss() -> None:
    """Test that resolver is called when not in cache."""
    cache = AddressCache(mdns_cache={"other.local": ["192.168.1.200"]})

    mock_addr_info = AddrInfo(
        family=socket.AF_INET,
        type=socket.SOCK_STREAM,
        proto=socket.IPPROTO_TCP,
        sockaddr=IPv4Sockaddr(address="192.168.1.100", port=6053),
    )

    with patch("esphome.resolver.AsyncResolver") as MockResolver:
        mock_resolver = MockResolver.return_value
        mock_resolver.resolve.return_value = [mock_addr_info]

        result = helpers.resolve_ip_address("test.local", 6053, address_cache=cache)

        # Should call resolver since test.local is not in cache
        MockResolver.assert_called_once_with(["test.local"], 6053)
        assert len(result) == 1
        assert result[0][4][0] == "192.168.1.100"


def test_resolve_ip_address_mixed_cached_uncached() -> None:
    """Test resolution with mix of cached and uncached hosts."""
    cache = AddressCache(mdns_cache={"cached.local": ["192.168.1.50"]})

    mock_addr_info = AddrInfo(
        family=socket.AF_INET,
        type=socket.SOCK_STREAM,
        proto=socket.IPPROTO_TCP,
        sockaddr=IPv4Sockaddr(address="192.168.1.100", port=6053),
    )

    with patch("esphome.resolver.AsyncResolver") as MockResolver:
        mock_resolver = MockResolver.return_value
        mock_resolver.resolve.return_value = [mock_addr_info]

        # Pass a list with cached IP, cached hostname, and uncached hostname
        result = helpers.resolve_ip_address(
            ["192.168.1.10", "cached.local", "uncached.local"],
            6053,
            address_cache=cache,
        )

        # Should only resolve uncached.local
        MockResolver.assert_called_once_with(["uncached.local"], 6053)

        # Results should include all addresses
        addresses = [r[4][0] for r in result]
        assert "192.168.1.10" in addresses  # Direct IP
        assert "192.168.1.50" in addresses  # From cache
        assert "192.168.1.100" in addresses  # From resolver
