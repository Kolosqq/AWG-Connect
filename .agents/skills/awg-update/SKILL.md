---
name: awg-update
description: Update the AWG Connect fork to the latest stable official AmneziaVPN release, integrate upstream Git changes, preserve fork behavior, and verify the Windows build and VPN.
---

1. Verify the latest published stable release in official [AmneziaVPN Releases](https://github.com/amnezia-vpn/amnezia-client/releases). Exclude drafts and prereleases. Verify the `upstream` remote against that repository and fetch the exact release tag. Resolve it to a commit and record the release, fork and upstream SHAs, and merge base.
2. Follow [integration.md](references/integration.md) to merge and review compatibility. Build the Windows client, service and affected test targets with the repository CI recipe. Run the [shared VPN regression matrix](../awg-development/references/vpn-regression.md) on the integrated candidate, including real traffic and Kill Switch behavior. Report precisely which checks need a VM or real VPN server if either is unavailable.
3. Prepare the reviewed Git changes for the working branch. Report the upstream version, conflicts, verification and gaps. Follow `AGENTS.md` for commit and push authorization.
