from esphome.const import __version__
from esphome.core import CORE
from esphome.core.config import _get_ccache_data
from esphome.helpers import mkdir_p, read_file, write_file_if_changed
from esphome.writer import find_begin_end, update_storage_json

INI_AUTO_GENERATE_BEGIN = "; ========== AUTO GENERATED CODE BEGIN ==========="
INI_AUTO_GENERATE_END = "; =========== AUTO GENERATED CODE END ============"

INI_BASE_FORMAT = (
    """; Auto generated code by esphome

[common]
lib_deps =
build_flags =
upload_flags =

""",
    """

""",
)


def format_ini(data: dict[str, str | list[str]]) -> str:
    content = ""
    for key, value in sorted(data.items()):
        if isinstance(value, list):
            content += f"{key} =\n"
            for x in value:
                content += f"    {x}\n"
        else:
            content += f"{key} = {value}\n"
    return content


def get_ini_content():
    CORE.add_platformio_option(
        "lib_deps",
        [x.as_lib_dep for x in CORE.platformio_libraries.values()]
        + ["${common.lib_deps}"],
    )
    # Sort to avoid changing build flags order
    CORE.add_platformio_option("build_flags", sorted(CORE.build_flags))

    # Sort to avoid changing build unflags order
    CORE.add_platformio_option("build_unflags", sorted(CORE.build_unflags))

    # Add extra script for C++ flags
    CORE.add_platformio_option("extra_scripts", [f"pre:{CXX_FLAGS_FILE_NAME}"])

    if _get_ccache_data().enabled:
        CORE.add_platformio_option("extra_scripts", [f"post:{CCACHE_SCRIPT_FILE_NAME}"])

    content = "[platformio]\n"
    content += f"description = ESPHome {__version__}\n"

    content += f"[env:{CORE.pio_env_name}]\n"
    content += format_ini(CORE.platformio_options)

    return content


def write_ini(content):
    update_storage_json()
    path = CORE.relative_build_path("platformio.ini")

    if path.is_file():
        text = read_file(path)
        content_format = find_begin_end(
            text, INI_AUTO_GENERATE_BEGIN, INI_AUTO_GENERATE_END
        )
    else:
        content_format = INI_BASE_FORMAT
    full_file = f"{content_format[0] + INI_AUTO_GENERATE_BEGIN}\n{content}"
    full_file += INI_AUTO_GENERATE_END + content_format[1]
    write_file_if_changed(path, full_file)


def write_project():
    mkdir_p(CORE.build_path)

    content = get_ini_content()
    write_ini(content)

    # Write extra script for C++ specific flags
    write_cxx_flags_script()

    if _get_ccache_data().enabled:
        write_ccache_script()


CXX_FLAGS_FILE_NAME = "cxx_flags.py"
CXX_FLAGS_FILE_CONTENTS = """# Auto-generated ESPHome script for C++ specific compiler flags
Import("env")

# Add C++ specific flags
"""


def write_cxx_flags_script() -> None:
    path = CORE.relative_build_path(CXX_FLAGS_FILE_NAME)
    contents = CXX_FLAGS_FILE_CONTENTS
    if not CORE.is_host:
        contents += 'env.Append(CXXFLAGS=["-Wno-volatile"])'
        contents += "\n"
    write_file_if_changed(path, contents)


CCACHE_SCRIPT_FILE_NAME = "ccache_wrapper.py"
CCACHE_SCRIPT_CONTENTS = """\
# Auto-generated ESPHome script for ccache compiler caching
import shutil
Import("env")

ccache_path = shutil.which("ccache")
if ccache_path:
    def _wrap_env(e):
        cc = e["CC"]
        if isinstance(cc, list) and cc[0] == "ccache":
            return
        e.Replace(CC=["ccache", cc], CXX=["ccache", e["CXX"]])

    _wrap_env(env)
    try:
        Import("projenv")
        _wrap_env(projenv)
    except Exception:
        pass
    for lb in env.GetLibBuilders():
        _wrap_env(lb.env)
    print(f"ccache: wrapping CC/CXX with {ccache_path}")
"""


def write_ccache_script() -> None:
    path = CORE.relative_build_path(CCACHE_SCRIPT_FILE_NAME)
    write_file_if_changed(path, CCACHE_SCRIPT_CONTENTS)
