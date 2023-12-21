import pytest

from hypothesis import given
from hypothesis.provisional import ip_addresses

from esphome import helpers


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
    assert all(p.startswith(str(path)) for p in actual)


class Test_write_file_if_changed:
    def test_src_and_dst_match(self, tmp_path):
        text = "A files are unique.\n"
        initial = text
        dst = tmp_path / "file-a.txt"
        dst.write_text(initial)

        helpers.write_file_if_changed(dst, text)

        assert dst.read_text() == text

    def test_src_and_dst_do_not_match(self, tmp_path):
        text = "A files are unique.\n"
        initial = "B files are unique.\n"
        dst = tmp_path / "file-a.txt"
        dst.write_text(initial)

        helpers.write_file_if_changed(dst, text)

        assert dst.read_text() == text

    def test_dst_does_not_exist(self, tmp_path):
        text = "A files are unique.\n"
        dst = tmp_path / "file-a.txt"

        helpers.write_file_if_changed(dst, text)

        assert dst.read_text() == text


class Test_copy_file_if_changed:
    def test_src_and_dst_match(self, tmp_path, fixture_path):
        src = fixture_path / "helpers" / "file-a.txt"
        initial = fixture_path / "helpers" / "file-a.txt"
        dst = tmp_path / "file-a.txt"

        dst.write_text(initial.read_text())

        helpers.copy_file_if_changed(src, dst)

    def test_src_and_dst_do_not_match(self, tmp_path, fixture_path):
        src = fixture_path / "helpers" / "file-a.txt"
        initial = fixture_path / "helpers" / "file-c.txt"
        dst = tmp_path / "file-a.txt"

        dst.write_text(initial.read_text())

        helpers.copy_file_if_changed(src, dst)

        assert src.read_text() == dst.read_text()

    def test_dst_does_not_exist(self, tmp_path, fixture_path):
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
    ),
)
def test_sanitize(text, expected):
    actual = helpers.sanitize(text)

    assert actual == expected
