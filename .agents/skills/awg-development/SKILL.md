---
name: awg-development
description: Use for any AWG Connect development or maintenance task, including features, bug fixes, GUI, performance, networking, VPN tests, Windows builds, commits, pushes, and GitHub release preparation. Use awg-update for stable AmneziaVPN upstream integration.
---

- Start with `AGENTS.md` and the affected entry points. Implement the smallest complete change. Choose build and test targets by risk, reuse current evidence for the same code and environment, and distinguish passes, failures, skips and unrun checks.
- Read [vpn-regression.md](references/vpn-regression.md) for VPN, routing, split tunneling, or Kill Switch changes and when asked for a network regression check.
- Read [windows-release.md](references/windows-release.md) only when preparing or publishing a release. Prepare the exact candidate and require explicit approval before any release or release tag is created remotely.
- Review the diff for scope and secrets. For completed, verified changes, use a scoped conventional commit and push to the current working branch under `AGENTS.md`. Keep user edits out of the commit and preserve published history. Keep the report to the result, evidence and gaps.
