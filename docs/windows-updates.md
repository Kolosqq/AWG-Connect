# Windows updates

Windows checks the latest stable GitHub Release for Kolosqq/AWG-Connect and uses the fork version from CMake. If the fork is current, the app checks the official AmneziaVPN release for an informational notice.

The published 1.1.1 asset is an offline Qt IFW EXE. The automatic channel uses the existing WiX build and accepts a single signed MSI named AWG-Connect_<version>_windows_x64.msi in release v<version>. CI produces both installer formats. Existing IFW users need to uninstall the IFW application and install the MSI once, with their existing user settings retained. Do not install IFW and MSI side by side. Once on MSI, Windows Installer upgrades the existing installation.

Before distributing the first updater-enabled build, obtain the publisher's Authenticode certificate and set AWG_UPDATE_SIGNER_SHA256 to the SHA-256 of the leaf certificate (DER bytes). Configure it as a GitHub repository variable or a CMake cache value. This value is public. Keep the private key in the publisher's signing environment. An empty pin disables automatic installation. Sign each MSI after packaging using the existing signtool path or an approved signing service. Check its signature, signer fingerprint, MSI version and SHA-256 before attaching it to the approved release. Never upload unsigned MSI candidates to the automatic channel. A certificate change requires distributing a client that trusts the new certificate before using it for updates.

The runner waits for the client to exit, locks the downloaded MSI against file writes and deletion, verifies the signature and package metadata again, and starts Windows Installer. Windows Installer manages elevation and service replacement. The package has an embedded cabinet and its removal of the old product runs inside the rollback transaction. User settings, keychain entries and VPN configs are outside the MSI installation directory. Windows Installer rollback covers a failed install transaction. It does not roll back a successful install after later application errors, and must be enabled by Windows policy. The runner records the Windows Installer exit code in result.txt in its temporary update directory.

## VM acceptance

Use disposable Windows snapshots and test packages signed by a test publisher trusted only by the VM. Build the client with that test certificate fingerprint. Start with a lower fork MSI version and import a test VPN profile and split list. Use a higher signed MSI for the update.

1. Verify offline and malformed GitHub responses, stable version ordering, no update for equal or older versions, and informational notices for official releases.
2. Test cancelled and interrupted downloads, corrupted MSI, wrong publisher, wrong version and wrong upgrade family. Confirm no installation or elevation.
3. Approve a valid update with VPN connected. Confirm tunnel shutdown, exit of the old client, Windows Installer elevation at install time, service startup and retained settings after restart.
4. Inject a failing MSI installation operation after old-product removal and before InstallFinalize in a test-only package. Confirm Windows Installer restores the old files and service and that the previous client reconnects with the same profile and split list. Repeat with UAC cancellation. Do not change rollback policy.
5. Exercise the IFW-to-MSI migration, MSI uninstall cleanup and VPN regression matrix before distributing the first signed build.
