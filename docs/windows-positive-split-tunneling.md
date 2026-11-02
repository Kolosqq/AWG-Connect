# Windows positive split tunneling

## Baseline

- Upstream: https://github.com/amnezia-vpn/amnezia-client
- Upstream commit: `3f6ecacff15e0ada890f6cf5174602d2b6b899c5` (`dev`).
- Fork branch: `feature/awg-split-tunneling`.
- All four submodules checked out at the upstream gitlinks.
- Unmodified Windows build: **passed**, job `Build-Windows` in
  https://github.com/Kolosqq/AWG-Connect/actions/runs/41459625586.
- The full upstream workflow also runs unrelated platforms and tests requiring
  upstream secrets; its overall status is not the Windows build result.
- Local baseline build stopped before compilation: CMake, MSVC and Qt are not
  installed in the detected locations or available on PATH.

## Existing implementation

- `client/ui/qml/Pages2/PageSettingsAppSplitTunneling.qml`: existing two-mode
  selector, currently enabled only on Android.
- `client/core/utils/routeModes.h`: `VpnOnlyForwardApps` already exists.
- `client/vpnConnection.cpp`: stores the mode and application list in the VPN
  configuration, but an empty application list resets the mode to all apps.
- `client/mozilla/localsocketcontroller.cpp`: currently transmits the application
  list as `vpnDisabledApps` and discards the mode for the desktop daemon.
- `client/platforms/windows/daemon/windowsdaemon.cpp`: starts the existing
  application-exclusion implementation.
- `client/platforms/windows/daemon/windowssplittunnel.cpp`: Mullvad IOCTL client.
- `recipes/win-split-tunnel/conanfile.py`: pinned driver version `1.2.5.0`;
  source tag commit `198ecde6e8cf547a01810b9ae87cefba562f7f99` in
  https://github.com/mullvad/win-split-tunnel.

The driver has no positive-mode flag. Its address-based bind/connect redirect
may be reusable with reversed interface roles, provided routing, firewall, DNS,
IPv6 and connection lifecycle are adapted and verified together. This is an
implementation candidate, not a demonstrated working positive mode.

## Host testing constraints

The user has authorized testing on the current Windows host instead of a VM.
An existing AWG 3.1 connection must remain active. Do not install or start the
fork's service, change routes, stop the existing tunnel, or perform connection
tests automatically on this host. Hand over any test requiring a VPN switch to
the user. Do not change Windows security settings or install experimental
drivers without confirmation.

No positive-mode traffic test has passed yet.
