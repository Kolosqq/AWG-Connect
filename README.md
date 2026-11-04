# AWG-Connect

[Русский](README_RU.md) · **[Download 1.0.0 for Windows x64](https://github.com/Kolosqq/AWG-Connect/releases/download/v1.0.0/AWG-Connect_1.0.0_windows_x64.exe)**

A Windows fork of [AmneziaVPN](https://github.com/amnezia-vpn/amnezia-client) with **positive app split tunneling**: selected applications use the VPN, everything else connects directly. The original interface and existing signed tunnel driver are reused.

## Version 1.0.0

- Select application `.exe` files for the **Via VPN** list.
- Supports AmneziaWG and WireGuard; Premium keys and native `.conf` import retain the original workflow.
- Starts without a console window; incompatible settings show an error instead of hanging.

To use positive mode, turn off both Kill Switch modes and site split tunneling, then select applications and connect. The configuration must use IPv4 default AllowedIPs (`0.0.0.0/0`, optionally with `::/0`). An empty selection sends everything directly. System DNS remains direct; this version does not provide an app-specific Kill Switch.

The installer retains AmneziaVPN's application/service names and replaces an existing installation. The installer is unsigned; the bundled driver is signed. `1.0.0` is the fork release version; the application retains upstream version `5.1.0.5`.

[Releases and checksums](https://github.com/Kolosqq/AWG-Connect/releases) · [Build and test notes](docs/windows-positive-split-tunneling.md)

Folder selection, selection from running processes and compatible Kill Switch behavior are planned for the next beta; they are not part of 1.0.0.

Based on AmneziaVPN. Distributed under [GPLv3](LICENSE); upstream attribution and third-party licenses are retained.
