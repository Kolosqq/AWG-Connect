# Integrate the stable upstream release

- Check branch, status and remotes. The official remote is `upstream` at `https://github.com/amnezia-vpn/amnezia-client.git`. The fork is `origin` at `Kolosqq/AWG-Connect`.
- Compare each side's commits and changed paths from the merge base. Start with overlap and inspect the shared interfaces that changed even if Git merges them cleanly. Inventory fork CI, packaging, branding and documentation alongside VPN behavior.
- Use an isolated branch or worktree when the current tree has edits or the integration needs review. Prefer a merge for published history. Resolve using the base and both sides. Retain upstream fixes and adapt fork behavior to upstream architecture where necessary. Stop and identify any conflict that cannot be reconciled safely.
- Trace saved settings, imports and config migration, AWG/WireGuard, both app split modes, site split, app routing and Kill Switch through the shared config, IPC capability negotiation, client/service/driver interfaces, routes, DNS and WFP. Preserve fail-safe behavior and cleanup. Check upgrades with existing settings and configs.
- Review the integrated diff for dropped fork behavior and unintended changes. Verify shared config paths on affected consumers, including native, self-hosted and Premium/Free. A clean merge is only a Git result.
