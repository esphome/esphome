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
if(EXISTS "${_matter_dir}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env python3
                "${CMAKE_CURRENT_LIST_DIR}/_apply_patches.py" "${_matter_dir}"
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
        message(STATUS "matter: source patches applied to ${_matter_dir}")
    endif()
else()
    message(STATUS
        "matter: ${_matter_dir} missing — component-manager has not synced yet")
endif()
