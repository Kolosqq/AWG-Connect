---
name: awg-upstream-update
description: Integrate official AmneziaVPN changes into the AWG Connect fork, analyze merge conflicts, and preserve the fork's Windows split tunneling, build, and branding changes.
---

1. Check branch, status, and remotes. The official remote is `upstream` at `https://github.com/amnezia-vpn/amnezia-client.git`; the fork is `origin` at `Kolosqq/AWG-Connect`. Fetch the intended upstream ref. Use `upstream/dev` for development integration unless a different branch, tag, or SHA was requested. Record both SHAs and their merge base.
2. Compare fork and upstream commits and changed paths from the merge base. Start with overlapping files and relevant commits. Also trace changes to shared config, IPC/service capabilities and Windows driver, route and firewall interfaces even where Git merges cleanly. Include fork CI, packaging, branding and documentation in the inventory.
3. Integrate on an isolated branch or worktree if the current tree has edits or the integration needs review. Prefer a merge for the published fork branch. Resolve from the base and both sides, retaining upstream fixes and the fork behavior in `AGENTS.md`. Check selection settings, driver/IPC compatibility and failure cleanup across boundaries. Stop with the specific conflict if either side's behavior cannot be reconciled safely.
4. Review the merged diff and run risk-based checks. Use `awg-regression` when connection, routing, settings or firewall paths change. Summarize the integrated SHA, behavioral conflicts, checks and gaps. Follow the repository's commit and push authorization rules.
