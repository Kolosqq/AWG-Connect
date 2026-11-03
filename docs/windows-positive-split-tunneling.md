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
- The pinned x64 driver was downloaded as a file only: its SHA-256 matched
  `4056b22d08115c1a83bc2cafa17de0bb17db3705eac382de77fd7935eeff7edb`, and
  Windows Authenticode reported `Valid` (Mullvad VPN AB). It was not installed
  or loaded on the host.

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
Both prepared probe executables completed HTTPS requests under the existing VPN
and returned the same exit address. The probe correctly rejected this control
case as evidence of positive routing; no connection settings were changed.

For local regression binaries, set `QT_QPA_PLATFORM=offscreen` and
`AMNEZIA_TEST_IPC_URL=local:amnezia-isolated-regression-tests` to avoid contacting
the installed service, including its logging controls. The IPC override is
compiled only into test binaries. Set `AMNEZIA_TEST_QML_DIR` to the checkout's
`client/ui/qml` directory when running the portable QML regression test.

## Candidate implementation

The existing Via VPN selector is enabled on Windows. The mode is carried through
desktop IPC; the service reverses the driver address roles for selected apps.
Only default AllowedIPs are accepted, tunnel routes have a high metric, and the
existing route-capture feature stays off. A lower-priority WFP rule blocks
unselected traffic on the tunnel adapter. The driver retains its existing
process and child-process tracking.

Positive-mode reconnects recreate the interface and revalidate the configuration.
If any selected executable path cannot be converted for the driver, activation
fails rather than silently using a partial selection.
The client requires positive-mode support in the service status handshake; an
older service is rejected before sending an activation request.
Failed positive-mode activation stops application redirection and removes the
partially created interface through the existing deactivation path. This
privileged rollback still requires a live failure-path check.

An empty positive list keeps all applications direct. The ordinary exclusion
mode and its saved list remain separate. System DNS is left unchanged, including
the DNS setting passed to the tunnel library. Applications using Windows DNS
therefore resolve directly; their own DoH/DoT connections follow application
routing.

This first candidate supports AWG/WireGuard only, with IPv4 default AllowedIPs.
It rejects site split tunneling, Kill Switch and Strict Kill Switch instead of
silently changing them. It is not a per-application kill switch. Disconnection
removes application redirection. Runtime behavior on multiple physical adapters,
IPv6, explicit socket bindings, existing sockets and reconnects remains unverified.

## Manual traffic check

Do not run a second VPN connection over the existing AWG connection and count
that as a direct-routing test. The user must choose when to switch connections.
The installer uses upstream application/service names and may replace an
existing AmneziaVPN installation. Keep the original installer and a private
backup outside Git before switching. Do not upload configurations or test logs.

1. From the repository root, run
   `deploy/windows/Test-PositiveSplitTunnel.ps1 -Action Prepare`.
   This only creates two copies of Windows curl under ignored `.cache/`.
2. At a user-controlled time, obtain a known direct IPv4 exit reference with VPN
   disconnected, then install/start the candidate and import a configuration
   locally. No keys belong in the repository or chat.
3. Select **App split tunneling / Via VPN**. Add only the printed `vpn-probe.exe`
   path. Keep `direct-probe.exe` and its parent terminal outside the list.
   Use AWG/WireGuard with site split tunneling and both Kill Switch modes off.
4. Connect, then run
   `deploy/windows/Test-PositiveSplitTunnel.ps1 -Action Probe -ExpectedDirectIp <reference>`.
   Both checks must be true. Requests use HTTPS to api.ipify.org; the script
   prints comparisons only and does not store or print exit addresses.
5. Disconnect, clear the positive list, reconnect and confirm both applications
   use the direct reference. Equal exits are expected for this case; the Probe
   command intentionally reports that positive routing is not demonstrated.
6. Recheck exclusion mode, reconnect, applications started before connection,
   child processes, UDP/QUIC, IPv6, DNS, and network-adapter changes separately.
   The HTTPS probe does not test those cases or prove leak protection.

The probe script never changes VPN connections, routes, services, firewall rules
or Windows security settings.

The first application requested for a user test is Brave. Select its actual
`brave.exe` executable, fully exit its existing processes after saving work, and
restart it after enabling the candidate connection. Compare the browser's IPv4
exit with the VPN probe, while the unselected probe must retain the direct exit.
Keep the browser profile unchanged. Existing sockets, child processes and QUIC
need separate checks beyond the initial IPv4 HTTPS comparison.
