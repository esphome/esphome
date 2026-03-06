# Pre-build script: exclude esp-modbus test_apps, examples, and tools from the library build.
# - test_apps: Unity tests (need unity_fixture.h)
# - examples: sample apps
# - tools: mb_example_common has its own modbus_params.c; we use this component's modbus_params.h
# Also patches mb_slave.c, port_tcp_slave.c for shorter logs.
# Writes sdkconfig.defaults so CONFIG_VFS_SUPPORT_SELECT=y (fixes linker undefined reference
# to esp_vfs_select_triggered / esp_vfs_select_triggered_isr).
# Maintainer: Massimiliano
Import("env")
import os
import json
import shutil

# Set True to patch mb_slave.c for "Modbus request" / "Modbus response" INFO logs (fc, addr, count, len).
ENABLE_MODBUS_REQUEST_RESPONSE_LOGGING = False

# Merge into sdkconfig.defaults: VFS (required by esp-modbus) + force 4MB flash (board default is 2MB; overrides so "Expected 4MB, found 2MB" goes away).
_VFS_LINE = "CONFIG_VFS_SUPPORT_SELECT=y"
_FLASH_4MB = "CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y"
dst_defaults = os.path.join(env["PROJECT_DIR"], "sdkconfig.defaults")
existing = ""
if os.path.isfile(dst_defaults):
    with open(dst_defaults) as f:
        existing = f.read()

def drop(line):
    s = line.strip()
    return "CONFIG_VFS_SUPPORT_SELECT" in s or s.startswith("CONFIG_ESPTOOLPY_FLASHSIZE_")

lines = [l for l in existing.splitlines() if not drop(l)]
new_content = "\n".join(lines).rstrip()
if new_content:
    new_content += "\n"
new_content += "\n# modbus_slave_tcp: esp-modbus + 4MB flash\n"
new_content += _VFS_LINE + "\n" + _FLASH_4MB + "\n"
with open(dst_defaults, "w") as f:
    f.write(new_content)
print("filter_esp_modbus: merged CONFIG_VFS_SUPPORT_SELECT and CONFIG_ESPTOOLPY_FLASHSIZE_4MB into sdkconfig.defaults")
# Remove env-named sdkconfig so it is regenerated from sdkconfig.defaults (PlatformIO uses sdkconfig.<PIOENV>)
pioenv = env.get("PIOENV") or os.path.basename(env["PROJECT_DIR"].rstrip(os.sep))
for name in ("sdkconfig", "sdkconfig." + pioenv, "sdkconfig." + pioenv + ".esphomeinternal"):
    sdkconfig = os.path.join(env["PROJECT_DIR"], name)
    if os.path.isfile(sdkconfig):
        os.remove(sdkconfig)
        print("filter_esp_modbus: removed %s so it is regenerated from sdkconfig.defaults" % name)
# Force VFS to be rebuilt with new config (removes cached libvfs.a built without CONFIG_VFS_SUPPORT_SELECT)
if pioenv:
    vfs_dir = os.path.join(env["PROJECT_DIR"], ".pioenvs", pioenv, "esp-idf", "vfs")
    if os.path.isdir(vfs_dir):
        shutil.rmtree(vfs_dir)
        print("filter_esp_modbus: removed esp-idf/vfs build cache so VFS is rebuilt with CONFIG_VFS_SUPPORT_SELECT")

# Find esp-modbus library under .piolibdeps (library folder name = esp_modbus_lib from app; we discover by structure)
piolibdeps = os.path.join(env["PROJECT_DIR"], ".piolibdeps")
pioenv = env.get("PIOENV") or os.path.basename(env["PROJECT_DIR"].rstrip(os.sep))
lib_dir = None
if os.path.isdir(piolibdeps):
    # .piolibdeps/<pioenv>/<lib_name>/ or .piolibdeps/<lib_name>/; look for modbus/mb_controller + library.json
    for env_name in ([pioenv] if pioenv else []) + [""]:
        base = os.path.join(piolibdeps, env_name) if env_name else piolibdeps
        if not os.path.isdir(base):
            continue
        for lib_name in os.listdir(base):
            candidate = os.path.join(base, lib_name)
            if not os.path.isdir(candidate):
                continue
            if os.path.isfile(os.path.join(candidate, "library.json")) and os.path.isdir(os.path.join(candidate, "modbus", "mb_controller")):
                lib_dir = candidate
                break
        if lib_dir:
            break

if lib_dir is None:
    print("filter_esp_modbus: esp-modbus library not found under .piolibdeps, skipping")
else:
    lib_json = os.path.join(lib_dir, "library.json")
    if os.path.isfile(lib_json):
        with open(lib_json) as f:
            data = json.load(f)
        if "build" not in data:
            data["build"] = {}
        data["build"]["srcFilter"] = ["+<*>", "-<test_apps/>", "-<examples/>", "-<tools/>"]
        with open(lib_json, "w") as f:
            json.dump(data, f, indent=2)
        print("filter_esp_modbus: excluded test_apps/, examples/, tools/ from esp-modbus build")

    # mb_slave.c: apply or revert request/response logging so runtime matches ENABLE_MODBUS_REQUEST_RESPONSE_LOGGING
    old_request = """            mbs_obj->func_code = mbs_obj->frame[MB_PDU_FUNC_OFF];
            exception = mbs_check_invoke_handler(inst, mbs_obj->func_code, mbs_obj->frame, &mbs_obj->length);"""
    new_request = """            mbs_obj->func_code = mbs_obj->frame[MB_PDU_FUNC_OFF];
            {
                uint8_t fc = mbs_obj->func_code & (uint8_t)~MB_FUNC_ERROR;
                if (mbs_obj->length >= 5) {
                    ESP_LOGI(TAG, "Modbus request: fc=0x%02x addr=%u count=%u", (unsigned)fc,
                             (unsigned)((mbs_obj->frame[1] << 8) | mbs_obj->frame[2]),
                             (unsigned)((mbs_obj->frame[3] << 8) | mbs_obj->frame[4]));
                } else {
                    ESP_LOGI(TAG, "Modbus request: fc=0x%02x len=%u", (unsigned)fc, (unsigned)mbs_obj->length);
                }
            }
            exception = mbs_check_invoke_handler(inst, mbs_obj->func_code, mbs_obj->frame, &mbs_obj->length);"""
    old_response = """                if ((mbs_obj->cur_mode == MB_ASCII) && MB_ASCII_TIMEOUT_WAIT_BEFORE_SEND_MS) {
                    mb_port_timer_delay(MB_OBJ(inst->port_obj), MB_ASCII_TIMEOUT_WAIT_BEFORE_SEND_MS);
                }
                MB_PRT_BUF(inst->descr.parent_name, ":MB_SEND", (void *)mbs_obj->frame,"""
    new_response = """                if ((mbs_obj->cur_mode == MB_ASCII) && MB_ASCII_TIMEOUT_WAIT_BEFORE_SEND_MS) {
                    mb_port_timer_delay(MB_OBJ(inst->port_obj), MB_ASCII_TIMEOUT_WAIT_BEFORE_SEND_MS);
                }
                ESP_LOGI(TAG, "Modbus response: fc=0x%02x len=%u", (unsigned)mbs_obj->frame[0], (unsigned)mbs_obj->length);
                MB_PRT_BUF(inst->descr.parent_name, ":MB_SEND", (void *)mbs_obj->frame,"""

    mb_slave_c = os.path.join(lib_dir, "modbus", "mb_objects", "mb_slave.c")
    if os.path.isfile(mb_slave_c):
        with open(mb_slave_c, "r") as f:
            content = f.read()
        modified = False
        if ENABLE_MODBUS_REQUEST_RESPONSE_LOGGING:
            if old_request in content and new_request not in content:
                content = content.replace(old_request, new_request)
                modified = True
            if old_response in content and new_response not in content:
                content = content.replace(old_response, new_response)
                modified = True
            if modified:
                with open(mb_slave_c, "w") as f:
                    f.write(content)
                print("filter_esp_modbus: patched mb_slave.c for Modbus request/response logging")
        else:
            # Revert: remove our logging so with False we get no request/response logs
            if new_request in content:
                content = content.replace(new_request, old_request)
                modified = True
            if new_response in content:
                content = content.replace(new_response, old_response)
                modified = True
            if modified:
                with open(mb_slave_c, "w") as f:
                    f.write(content)
                print("filter_esp_modbus: reverted mb_slave.c request/response logging (ENABLE_MODBUS_REQUEST_RESPONSE_LOGGING=False)")

    # Shorten long TCP slave error lines so they are not truncated by the logger (comm fail / connection lost)
    port_tcp_slave_c = os.path.join(lib_dir, "modbus", "mb_ports", "tcp", "port_tcp_slave.c")
    if os.path.isfile(port_tcp_slave_c):
        with open(port_tcp_slave_c, "r") as f:
            content = f.read()
        old_comm_fail = """        ESP_LOGE(TAG, "%p, " MB_NODE_FMT(", communication fail, err= %d"),
                 port_obj, pnode->index, pnode->sock_id,
                 pnode->addr_info.ip_addr_str, (int)ret);"""
        new_comm_fail = """        ESP_LOGE(TAG, "master disconnected sock#%d err=%d",
                 (int)pnode->sock_id, (int)ret);"""
        old_conn_lost = """        ESP_LOGE(TAG, "%p, " MB_NODE_FMT(", connection lost, err=%d, drop connection."),
                 port_obj, pnode->index, pnode->sock_id,
                 pnode->addr_info.ip_addr_str, (int)ret);"""
        new_conn_lost = """        ESP_LOGE(TAG, "master disconnected sock#%d err=%d",
                 (int)pnode->sock_id, (int)ret);"""
        # Also upgrade already-patched messages to short "master disconnected" (fits log line limit)
        prev_comm_fail = """        ESP_LOGE(TAG, "master disconnected: node #%d sock(#%d)(%s) err=%d",
                 (int)pnode->index, (int)pnode->sock_id,
                 pnode->addr_info.ip_addr_str, (int)ret);"""
        prev_conn_lost = """        ESP_LOGE(TAG, "master disconnected: node #%d sock(#%d)(%s) err=%d",
                 (int)pnode->index, (int)pnode->sock_id,
                 pnode->addr_info.ip_addr_str, (int)ret);"""
        modified = False
        if old_comm_fail in content and new_comm_fail not in content:
            content = content.replace(old_comm_fail, new_comm_fail)
            modified = True
        elif prev_comm_fail in content and new_comm_fail not in content:
            content = content.replace(prev_comm_fail, new_comm_fail)
            modified = True
        if old_conn_lost in content and new_conn_lost not in content:
            content = content.replace(old_conn_lost, new_conn_lost)
            modified = True
        elif prev_conn_lost in content and new_conn_lost not in content:
            content = content.replace(prev_conn_lost, new_conn_lost)
            modified = True
        if modified:
            with open(port_tcp_slave_c, "w") as f:
                f.write(content)
            print("filter_esp_modbus: patched port_tcp_slave.c for shorter error lines")
