@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy RemoteSigned -File "%~dp0Check-CodexSplitTunnel.ps1" %*
pause
