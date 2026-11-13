# AWG-Connect

[Русский](README_RU.md) · **[Download 1.1.1 for Windows x64](https://github.com/Kolosqq/AWG-Connect/releases/download/v1.1.1/AWG-Connect_1.1.1_windows_x64.exe)**

A Windows fork of AmneziaVPN with positive split tunneling for AmneziaWG and WireGuard. Selected apps use the VPN. Other apps and system DNS connect directly.

- Select EXE files, folders including subfolders, or running processes. Selections are saved by executable path.
- Change split tunneling while connected. Confirm the change to save it and reconnect.
- Normal Kill Switch blocks selected apps after unexpected tunnel loss. Strict mode also blocks them after manual disconnect while the service is running.

Turn off site split tunneling and use IPv4 default AllowedIPs (`0.0.0.0/0`). An empty list sends all applications directly.

Release 1.1.1 is based on AmneziaVPN 5.1.0.6. The installer replaces an existing AmneziaVPN installation and retains its application and service names. The installer is unsigned. The bundled driver is signed.

[Release notes and checksum](https://github.com/Kolosqq/AWG-Connect/releases/tag/v1.1.1) · [Upstream AmneziaVPN](https://github.com/amnezia-vpn/amnezia-client) · [GPLv3](LICENSE)
