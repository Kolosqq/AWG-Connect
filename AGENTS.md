# AWG Connect

AWG Connect is a Windows-focused fork of AmneziaVPN (C++ and Qt Quick) with app inclusion split tunneling for AmneziaWG/WireGuard. Desktop clients use a privileged service over Qt Remote Objects. Mobile and macOS Network Extension builds have separate tunnel paths.

- `client/core/` owns config, import, settings and connection logic. `client/ui/{controllers,models,qml}/` adapts it to the UI.
- `client/daemon/` and `ipc/` carry desktop tunnel config and capabilities. `client/platforms/windows/daemon/` owns Windows routing and WFP integration, and `service/server/` owns Kill Switch policy.
- `client/tests/` holds Qt Test suites. `deploy/`, `recipes/` and `.github/workflows/` define builds, dependencies and CI.

## Build and verify

Use the Windows recipe in `.github/workflows/windows-split-tunneling.yml` (MSVC 2022 x64, Qt 6.10.1, Ninja, Conan, Qt Installer Framework). Match its gateway preparation, configure flags and test environment. For other platforms, start with `bash deploy/build.sh -h`.

From the root, after configuring with `AMNEZIA_BUILD_TESTS=ON`, build affected targets with `cmake --build deploy/build --target <targets>`. Run matching tests with `ctest --test-dir deploy/build/client/tests -C Release -R "<pattern>" --output-on-failure --no-tests=error`. Open touched QML pages or load them in a QML engine. Reconfigure cleanly for build or branding changes. Choose checks by risk, reuse existing builds and scripts, and report checks actually run and gaps.

## Guardrails

- Keep changes small and scoped. Read affected files first. Reuse existing logic, avoid unrelated refactors, and keep code self-documenting. Reserve new comments for non-obvious workarounds. Format touched C++ with `.clang-format`. Leave `client/3rd/` to upstream or Conan recipes.
- Preserve existing saved settings, exclude mode and ordinary VPN connections. In Windows inclusion mode, selected apps use the tunnel and unselected apps and system DNS stay direct. Preserve app-scoped Kill Switch behavior through failures and reconnects, client/service compatibility checks, and firewall/route cleanup. Verify shared config paths for self-hosted, Premium/Free and native configs on affected consumers.
- Follow Figma screens and use the existing QML design-system controls, icons and semantic colors. Move reusable pieces to `Components/` and use `QtQuick.Effects`. Use `qsTr()`/`tr()` for UI strings. Refresh translations only for translation tasks and validate all catalogs with `lrelease`.
- Keep credentials, private endpoints and full configs out of logs, reports and commits.
- Commit or push only when requested. Use scoped conventional commits and target `dev` for PRs. Creating a GitHub Release, including a draft, or pushing a release tag requires explicit approval for that release.
