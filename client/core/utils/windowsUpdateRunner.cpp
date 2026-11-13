#include "windowsUpdatePackage.h"

// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on

#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    bool run(const std::wstring &program, const std::wstring &arguments, DWORD *result = nullptr)
    {
        std::wstring command = L"\"" + program + L"\" " + arguments;
        STARTUPINFOW startup { };
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process { };
        if (!CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                            &process)) {
            return false;
        }
        CloseHandle(process.hThread);
        if (result) {
            WaitForSingleObject(process.hProcess, INFINITE);
            GetExitCodeProcess(process.hProcess, result);
        }
        CloseHandle(process.hProcess);
        return true;
    }
}

int main()
{
    int count = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &count);
    if (argv && count == 4 && std::wstring(argv[1]) == L"--verify-only") {
        const bool verified = windowsUpdate::verifyPackage(argv[2], argv[3]);
        LocalFree(argv);
        return verified ? 0 : 1;
    }
    if (!argv || count != 5) {
        LocalFree(argv);
        return 1;
    }
    const std::filesystem::path package(argv[1]);
    const std::wstring version(argv[2]);
    const std::filesystem::path client(argv[3]);
    wchar_t *end = nullptr;
    const DWORD pid = wcstoul(argv[4], &end, 10);
    const bool validPid = pid != 0 && end && *end == L'\0';
    LocalFree(argv);
    if (!validPid || !package.is_absolute() || package.extension() != L".msi" || !client.is_absolute()) {
        return 1;
    }
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (parent) {
        const auto status = WaitForSingleObject(parent, 60000);
        CloseHandle(parent);
        if (status != WAIT_OBJECT_0) {
            return 1;
        }
    } else if (GetLastError() != ERROR_INVALID_PARAMETER) {
        return 1;
    }

    if (version.empty() || version.find_first_not_of(L"0123456789.") != std::wstring::npos) {
        return 1;
    }
    DWORD result = ERROR_INSTALL_FAILURE;
    const HANDLE locked = CreateFileW(package.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (locked != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info { };
        if (GetFileInformationByHandle(locked, &info) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            && windowsUpdate::verifyPackage(package.wstring(), version)) {
            wchar_t system[MAX_PATH] { };
            const UINT length = GetSystemDirectoryW(system, MAX_PATH);
            if (length > 0 && length < MAX_PATH) {
                const auto arguments = L"/i \"" + package.wstring() + L"\" /passive /norestart INSTALL_ROOT=\""
                        + client.parent_path().wstring() + L"\"";
                run(std::wstring(system) + L"\\msiexec.exe", arguments, &result);
            }
        }
        CloseHandle(locked);
    }
    std::ofstream(package.parent_path() / "result.txt") << result << '\n';
    std::error_code cleanupError;
    std::filesystem::remove(package, cleanupError);
    std::filesystem::remove(package.wstring() + L".sig", cleanupError);
    if (result != ERROR_SUCCESS && result != ERROR_SUCCESS_REBOOT_REQUIRED && result != ERROR_SUCCESS_REBOOT_INITIATED) {
        MessageBoxW(
                nullptr,
                L"AWG-Connect update did not complete. Windows Installer result is in result.txt in the update folder.",
                L"AWG-Connect", MB_OK | MB_ICONWARNING);
    }
    if (result != ERROR_SUCCESS_REBOOT_REQUIRED && result != ERROR_SUCCESS_REBOOT_INITIATED) {
        run(client.wstring(), L"");
    }
    return result == ERROR_SUCCESS ? 0 : 1;
}
