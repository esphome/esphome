# ESPHome Threat Model

This document defines the trust boundary for the **ESPHome** repository — the
Python compiler/CLI and the device firmware it generates — so that real security
bugs can be told apart from defense-in-depth improvements. It gives contributors,
reviewers, and security researchers a clear answer to one question:
**does this issue let an _unauthenticated_ attacker do something they shouldn't?**

Related documents:

- Deployment guidance for operators:
  https://esphome.io/guides/security_best_practices/
- The **Device Builder dashboard** (the web UI, its authentication, ingress,
  Origin/Host gates, and peer-link pairing) lives in a separate repository and
  has its own threat model. If your report concerns any of that, please read and
  report there instead:
  https://github.com/esphome/device-builder/blob/main/docs/THREAT_MODEL.md

## The trust boundary

For this repository there are two trusted inputs by design:

1. **The configuration.** Anyone who can supply or edit a YAML config is trusted
   (see below).
2. **Authenticated peers of a running device** — clients holding the device's
   API/OTA encryption key, API password, OTA password, or web server
   credentials.

The security boundary is therefore **unauthenticated network traffic vs. those
trusted inputs.** A bug that lets an unauthenticated attacker cross it is a
security bug.

## Config authors are host-equivalent by design

Anyone who can supply or edit a configuration is **trusted with full code
execution on the host that runs `esphome`**, on purpose. This is what the product
does, not a flaw. A config author can already, through fully supported features:

- Run arbitrary **Python** at validation/compile time via `external_components:`
  (and other component-import mechanisms) — ESPHome imports those packages as
  ordinary Python.
- Run arbitrary **shell** commands through the compile/validate/flash toolchain
  that ESPHome invokes as subprocesses.
- Read and write arbitrary files reachable by the process (e.g. via `!include`,
  `packages:`, `dashboard_import:`, and generated build output).

Because of this, a malicious config author is equivalent to shell access on the
host running the build.

## What is *not* a security vulnerability

If exploiting an issue requires the ability to supply or edit configuration, it
is **not** a vulnerability in ESPHome, because that ability already grants host
code execution. This explicitly includes, among others:

- Template / expression injection in substitutions or any YAML string value
  (e.g. Jinja `${...}` evaluation reaching Python internals). This grants no
  capability a config author lacks.
- `!include` / `packages:` / `dashboard_import:` reading or fetching content
  from surprising or remote locations.
- The validator or compiler crashing or behaving unexpectedly on adversarial
  YAML.
- ESPHome running as root in the official container — that is the documented
  deployment posture, reachable by the same caller through the features above.

These do not warrant a CVE or coordinated disclosure. Hardening in these areas
(for example, sandboxing template evaluation as least-surprise defense-in-depth)
is welcome as a normal enhancement PR, framed as cleanliness rather than a
security fix — not as a vulnerability remediation.

## What we do defend

These *are* security bugs in this repo, and we want to hear about them privately:

- Memory-safety or protocol bugs in the generated **device firmware** that are
  remotely triggerable over the network (native API, web server, OTA, BLE,
  captive portal, etc.) **without** valid credentials.
- Authentication or encryption bypass on the device — reaching API calls, OTA
  updates, or the web server without the configured key/password.
- Flaws that weaken the device's API or OTA encryption (Noise), OTA auth, or
  web server auth below their documented guarantees.

## The web server is an open HTTP API by design

The `web_server` component exposes a plain HTTP interface for viewing and
controlling entities, and, when the `web_server` OTA platform is enabled, for
uploading firmware at `/update`. Its only access controls are the optional
`web_server` `auth:` credentials and the network the device sits on.

When `auth:` is not configured, every endpoint is reachable by any client that
can reach the device. This is intentional; enabling `web_server` without `auth:`
is choosing an open control surface, in the same way that running native OTA
without a password leaves OTA open. The API is documented and is meant to be
called by other devices, scripts, and pages.

As defense-in-depth, the web server checks the `Origin` header on browser requests
to its entity control and state endpoints: a request whose `Origin` does not match
the address the device is served on is rejected, and the `allowed_origins` option
widens that list. This blocks the common "confused deputy" (CSRF) case where a page
the operator visits drives the device through their browser. It is **not** an
authentication boundary: it only constrains browsers. Any client that omits the
`Origin` header — `curl`, scripts, or other non-browser callers on the same
network — reaches every endpoint exactly as before. The check also does not cover
the web OTA `/update` endpoint. The device performs no CSRF-token or `Referer`
validation. The following are therefore **not** vulnerabilities in this repository:

- Requests without an `Origin` header (for example `curl`) reaching the control
  endpoints, whether or not `web_server` `auth:` is set.
- Requests from an origin the operator added to `allowed_origins`.
- Cross-origin or CSRF firmware upload through the web OTA endpoint (`/update`) when
  web OTA is enabled without `web_server` `auth:`. The `/update` endpoint is not
  covered by the `Origin` check; this is the same exposure as running OTA without a
  password.

The supported defenses are `web_server` `auth:`, protecting OTA (a web password or
a native OTA password), and keeping devices on a trusted, segmented network. See
the security best practices guide linked above.

What remains in scope is bypassing `web_server` `auth:` when it *is* configured,
and any memory-safety or protocol bug in the server reachable without credentials.

This section documents the current design and scope; it is not a judgment that the
design is optimal or that it will not change.

## OTA update encryption

The `esphome` OTA platform optionally encrypts updates with the same Noise
`NNpsk0` pattern the native API uses; one key protects the device. A device
whose `api:` block has an encryption key, static in the YAML or provisioned at
runtime, compiles in the transport and offers it on every OTA connection once
it holds a key, so an uploader presenting that key gets the guarantees below
even without an `ota: encryption:` block; only that block makes the device
require encryption. The guarantees are: the firmware image is confidential in
transit, the uploader is authenticated by the pre-shared key, and the plaintext
negotiation preceding the handshake is bound into the handshake prologue, so
stripping or tampering with it fails the first MAC. With `ota: encryption:`
configured both ends fail closed with no override: the device refuses
plaintext uploads, and the CLI refuses to send plaintext when a key is
configured. Without that block the CLI tries a static api key when the device
offers and, until 2027.3.0, falls back to plaintext with a warning when the
offer is missing or the handshake fails; a runtime provisioned key never
reaches the CLI, so those uploads stay plaintext.

Defeating any of that without the key is in scope: a device that requires
encryption accepting a plaintext or downgraded upload, getting past the MAC,
or recovering image contents from captured traffic.

The following are **not** vulnerabilities, by design:

- Plaintext OTA on a device with no `ota: encryption:` block, including one
  that offers encryption because it has an api key. That is the documented
  default, authenticated (if at all) by the OTA password. An uploader that
  takes the offer skips the password; the key authenticates it.
- The CLI plaintext fallback until 2027.3.0: without `ota: encryption:` an
  active attacker who strips the offer or breaks the handshake can make a
  keyed CLI upload plaintext, with the pre-existing plaintext exposure. A
  device that requires encryption still refuses that upload.
- The enablement window: firmware built with a static api key already offers
  encryption, so turning on `ota: encryption:` is itself an encrypted upload.
  Older firmware needs one last plaintext upload of an offering build, with
  the pre-existing plaintext exposure.
- The web OTA `/update` endpoint alongside encryption. The `web_server` OTA
  platform keeps it always reachable and validation warns about that
  combination; `captive_portal:` auto-loads that platform only for the
  fallback AP window, which is the intended recovery path, so that alone is
  not warned about.
- CLI retry behavior on transport or MAC failures; every attempt renegotiates
  a fresh handshake with fresh ephemerals, so retrying does not weaken
  authentication.

## Explicitly out of scope

- Local attackers who already have shell access on the host that runs `esphome`.
- Supply-chain attacks against ESPHome or its dependencies.
- Operator-supplied hostile YAML (covered above — config authoring is trusted).
- Attacks that require an already-authenticated device peer (someone who already
  holds the API key / OTA / web credentials).
- Access to the device web server or its web OTA endpoint by non-browser clients
  (those that send no `Origin` header). The web server is an open HTTP API by
  design (see above); browser cross-origin requests are blocked by default, but the
  real controls are `web_server` `auth:` and network isolation.
- Anything in the dashboard / device-builder — report that in its own repository
  (linked at the top).
- Deployments where the operator removed protections or exposed credentials. See
  the security best practices guide:
  https://esphome.io/guides/security_best_practices/

## Reporting a vulnerability

If you believe you've found an issue that crosses the unauthenticated boundary
above, please report it privately via GitHub Security Advisories rather than a
public issue. For issues that require config-write access, please review this
document first — they are very likely out of scope by design. For dashboard /
device-builder issues, report against that repository and consult its threat
model (linked at the top).
