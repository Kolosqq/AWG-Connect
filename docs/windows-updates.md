# Windows updates

AWG-Connect checks stable releases in the fork repository. If the fork is current, the latest original AmneziaVPN release can appear as an informational notice. The Windows updater accepts the following pair of assets in release v<version>.

- AWG-Connect_<version>_windows_x64.msi
- AWG-Connect_<version>_windows_x64.msi.sig

Windows CNG verifies an RSA-3072 PSS signature over the entire MSI using SHA-256 and the public key built into the client and update runner. The binary signature is 384 bytes. The updater also checks the MSI version and the fork's upgrade identifier. The signature provides authenticity and integrity for AWG-Connect. Windows may still display an unknown publisher prompt for the MSI.

## Signing on the maintainer's Windows profile

The public key is in deploy/windows/update-public-key.hex. The private key is non-exportable and belongs to the local Windows user profile in the Microsoft Software Key Storage Provider. It is outside the repository and CI. This mechanism uses no Windows certificate store.

Use PowerShell 7 on the signing machine. For the existing signing profile, run the following from the repository root after verifying the CI artifact, exact commit, version and package metadata.

    pwsh -File deploy/windows/verify-update-msi.ps1 -BuildDirectory <candidate-directory>
    pwsh -File deploy/windows/sign-update.ps1 -Package <candidate-msi>

Attach the MSI and its .sig to the approved release along with the SHA-256 list. Build with AWG_CONNECT_VERSION equal to the numeric part of the release tag. CI builds the package but cannot sign it. Review the exact candidate before publishing. Run the installed update runner with --verify-only <msi> <version> to check a package without installation. The expected exit code is 0 for a valid package and 1 for a rejected package.

If the Windows profile or key is lost, distribute a new bootstrap client with a new public key through a trusted channel. Replacing the public key in GitHub alone cannot change the key pinned by existing installations.

## Acceptance on Windows 11

Each installation, removal or interruption of the active VPN on the main computer needs the user's approval. Use a time when a temporary network interruption is acceptable. First verify offline checks and tamper detection without installing anything.

1. Make a private backup with the existing export tools and retain the previous installer. Keep the original Windows user account and its credential store. Record the current client version, VPN profile and split-tunneling rules without copying keys into logs.
2. For the first transition from the published Qt IFW EXE, close the client and remove the IFW installation. Install the approved bootstrap MSI under the same Windows account and verify the retained profile and rules. Do not install the two packages side by side.
3. Check updates with the network available. Use a local test feed or approved staging release to present a higher version. Test the damaged MSI or signature in the verifier first. Neither should reach Windows Installer. For the offline test, use a local unavailable endpoint in the test harness or perform the test during a user-approved network interruption.
4. On a signed update, check that the client shuts down the tunnel and exits. Windows Installer replaces the service and starts it. Verify that the client reopens and that the profile and split rules are present. Reconnect and repeat the normal and strict Kill Switch checks.
5. Test UAC cancellation separately. To exercise transaction rollback, use a dedicated test package and an isolated disposable CI runner. Avoid deliberately failing an installation on the main PC.

Windows Installer restores the old files and service if installation fails inside its rollback transaction. Rollback must be enabled in Windows policy. The updater leaves the user configuration outside the package and relaunches the client after a failed installation when reboot is unnecessary. The runner records the Windows Installer exit code in result.txt in its temporary update directory. A failure after a successful transaction requires manual recovery with the saved installer and user backup.
