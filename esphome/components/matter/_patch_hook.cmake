# Injected via `-DCMAKE_PROJECT_INCLUDE=<abs path to this file>` from
# components/matter/__init__.py. CMAKE_PROJECT_INCLUDE runs AFTER the
# top-level `project()` call, which itself happens AFTER
# `include($IDF_PATH/tools/cmake/project.cmake)` has already invoked
# `idf_build_process()` → idf-component-manager sync (which downloads and
# integrity-restores managed_components/). This is the ONLY configure-time
# hook that fires after component-manager but before ninja starts compiling,
# so it's where source-level patches must land.
#
# CMake configure re-runs whenever inputs change (CMakeLists, sdkconfig,
# dependencies.lock, ...). On each such re-run component-manager may revert
# our files; this hook then re-patches them in the same configure pass, so
# ninja never sees an unpatched source. When configure is skipped
# (day-to-day rebuilds), the previous configure's patches remain in place.

set(_matter_dir "${CMAKE_HOME_DIRECTORY}/managed_components/espressif__esp_matter")
# Resolve the Python interpreter the way ESP-IDF itself does — the `python3`
# alias is not present on all systems (notably the ESP-IDF Windows install
# and some containerised setups). ESP-IDF exposes the resolved interpreter
# via ${python} on modern releases and via ${Python3_EXECUTABLE} when the
# newer FindPython machinery ran. Fall back to `python3` only if neither is
# defined so a stock Linux install still works when the hook is exercised
# outside of an ESP-IDF configure (e.g. by our own test scripts).
if(DEFINED python)
    set(_matter_python "${python}")
elseif(DEFINED Python3_EXECUTABLE)
    set(_matter_python "${Python3_EXECUTABLE}")
else()
    set(_matter_python "python3")
endif()
set(_matter_stamp "${_matter_dir}/.esphome-matter-patched")
if(EXISTS "${_matter_dir}")
    execute_process(
        COMMAND "${_matter_python}" "${CMAKE_CURRENT_LIST_DIR}/_apply_patches.py" "${_matter_dir}"
        RESULT_VARIABLE _matter_patches_rc
        OUTPUT_VARIABLE _matter_patches_out
        ERROR_VARIABLE _matter_patches_err
    )
    # rc=20 is the distinct "esp_matter dir not present yet" tolerated skip
    # (see _apply_patches.py::main). Anything else non-zero means the sweep
    # or an anchored patch failed against the current esp-matter release —
    # e.g. PATCH3's absence leaves std::unordered_map in place, which links
    # fine but reproduces the registry-corruption bug at runtime. Escalate
    # to FATAL_ERROR so a green build cannot ship silently-broken firmware.
    if(_matter_patches_rc EQUAL 20)
        message(STATUS
            "matter: esp_matter dir not populated yet — will retry next configure")
    elseif(NOT _matter_patches_rc EQUAL 0)
        message(FATAL_ERROR
            "matter: _apply_patches.py failed rc=${_matter_patches_rc}\n"
            "stdout: ${_matter_patches_out}\n"
            "stderr: ${_matter_patches_err}")
    else()
        # Positive marker so drift detection (a future esp_matter release
        # that moves the managed-component path, or a stale build tree
        # where the dir survives but our patches were reverted) can spot
        # "compiled without patches" state by looking for this stamp.
        file(WRITE "${_matter_stamp}" "applied\n")
        message(STATUS "matter: source patches applied to ${_matter_dir}")
    endif()
else()
    # Track visits so a real upstream path drift can't silently pass. First
    # configure with a missing dir is tolerated (component-manager may not
    # have synced yet on cold caches); the second is fatal — otherwise
    # ninja compiles unpatched sources behind an easy-to-miss warning.
    set(_matter_miss_stamp "${CMAKE_BINARY_DIR}/.esphome-matter-dir-missing")
    if(EXISTS "${_matter_miss_stamp}")
        message(FATAL_ERROR
            "matter: ${_matter_dir} still missing on the second configure — "
            "either component-manager cannot sync espressif/esp_matter, or "
            "the managed-component path drifted upstream. Refusing to compile "
            "unpatched sources.")
    else()
        file(WRITE "${_matter_miss_stamp}" "1\n")
        message(AUTHOR_WARNING
            "matter: ${_matter_dir} missing — component-manager has not synced "
            "yet. If configure re-runs cleanly next time, ignore; if the next "
            "configure still finds it missing, the build will fail.")
    endif()
endif()

# Clear the miss stamp on any successful pass so subsequent missing-dir
# events start a fresh two-strike counter rather than tripping on state
# left over from a long-ago cold configure.
if(EXISTS "${_matter_dir}" AND EXISTS "${CMAKE_BINARY_DIR}/.esphome-matter-dir-missing")
    file(REMOVE "${CMAKE_BINARY_DIR}/.esphome-matter-dir-missing")
endif()
