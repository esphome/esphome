/*
 * Linker wrap stubs for the lwIP2 glue's dead DHCP entry points.
 *
 * The ESP8266 SDK blobs call dhcp_cleanup() and dhcp_release() when the
 * station leaves an access point (cnx_sta_leave, wifi_station_dhcpc_stop).
 * In the prebuilt lwIP2 glue (liblwip2-*.a, glue-esp/lwip-esp.c) these are
 * stubs whose only effect is printing "STUB: dhcp_cleanup" and
 * "STUB: dhcp_release"; the real DHCP teardown happens through lwIP2's
 * renamed dhcp_cleanup_LWIP2()/dhcp_release_LWIP2() functions.
 *
 * On ESP8266 .rodata lives in DRAM, so those message strings waste scarce
 * RAM. Wrapping the stubs with silent equivalents lets the linker garbage
 * collect the glue stub bodies together with their strings.
 *
 * Saves 38 bytes of RAM and removes the "STUB:" log noise on Wi-Fi
 * disconnect. Behavior is otherwise unchanged.
 */

#if defined(USE_ESP8266)

namespace esphome::esp8266 {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

// The callers are closed-source SDK blobs; the netif argument is unused.
void __wrap_dhcp_cleanup(void * /*netif*/) {}

// The glue stub returns ERR_ABRT (-8; lwIP 1.4 err_t is a signed char).
signed char __wrap_dhcp_release(void * /*netif*/) { return -8; }

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP8266
