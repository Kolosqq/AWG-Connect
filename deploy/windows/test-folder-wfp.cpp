// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <initguid.h>
#include <fwpmu.h>
#include <rpc.h>
#include <iphlpapi.h>
#include <netioapi.h>
// clang-format on

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "../../client/platforms/windows/daemon/appfirewall.h"

namespace fs = std::filesystem;

int probe(unsigned short port)
{
    const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in target { };
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = htonl(0x01010101);
    const int result = connect(client, reinterpret_cast<sockaddr *>(&target), sizeof(target));
    closesocket(client);
    return result == 0 ? 0 : 2;
}

DWORD runProbe(const fs::path &path, unsigned short port)
{
    std::wstring command = L"\"" + path.wstring() + L"\" --probe " + std::to_wstring(port);
    STARTUPINFOW startup { };
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process { };
    if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        path.parent_path().c_str(), &startup, &process)) {
        return 100;
    }
    if (WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 101);
        WaitForSingleObject(process.hProcess, 5000);
    }
    DWORD code = 102;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code;
}

int main(int argc, char **argv)
{
    WSADATA sockets { };
    if (WSAStartup(MAKEWORD(2, 2), &sockets)) {
        return 1;
    }
    if (argc == 3 && std::string(argv[1]) == "--probe") {
        return probe(static_cast<unsigned short>(std::stoi(argv[2])));
    }
    wchar_t executable[32768];
    GetModuleFileNameW(nullptr, executable, 32768);
    wchar_t temporaryDirectory[32768];
    if (!GetLongPathNameW(fs::temp_directory_path().c_str(), temporaryDirectory, 32768)) {
        return 1;
    }
    const fs::path root = fs::path(temporaryDirectory) / (L"awg-wfp-test-" + std::to_wstring(GetCurrentProcessId()));
    const fs::path selected = root / L"SELECTED_\u041F\u0410\u041F\u041A\u0410" / L"nested" / L"probe.exe";
    const fs::path direct = root / L"SELECTED_\u041F\u0410\u041F\u041A\u0410-other" / L"probe.exe";
    fs::create_directories(selected.parent_path());
    fs::create_directories(direct.parent_path());
    fs::copy_file(executable, selected);
    fs::copy_file(executable, direct);
    const unsigned short port = 443;
    HANDLE engine = nullptr;
    FWPM_SESSION0 session { };
    session.flags = FWPM_SESSION_FLAG_DYNAMIC;
    DWORD status = FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, &session, &engine);
    bool passed = false;
    if (status == ERROR_SUCCESS) {
        GUID key;
        UuidCreate(&key);
        FWPM_SUBLAYER0 sublayer { };
        sublayer.subLayerKey = key;
        sublayer.displayData.name = const_cast<wchar_t *>(L"AWG isolated folder test");
        sublayer.weight = 65535;
        status = FwpmSubLayerAdd0(engine, &sublayer, nullptr);
        std::vector<UINT64> filters;
        const bool baseline = runProbe(selected, port) == 0 && runProbe(direct, port) == 0;
        const auto folder = selected.parent_path().parent_path();
        status = appFirewall::replace(engine, key, { { folder.wstring(), true } }, 0, filters);
        const bool folderBlocked = status == ERROR_SUCCESS && runProbe(selected, port) == 2;
        const bool siblingAllowed = runProbe(direct, port) == 0;
        const auto added = folder / L"added.exe";
        fs::copy_file(executable, added);
        const bool newFileBlocked = runProbe(added, port) == 2;
        status = appFirewall::replace(engine, key, { { selected.wstring(), false } }, 0, filters);
        const bool exactBlocked = status == ERROR_SUCCESS && runProbe(selected, port) == 2;
        const bool otherFileAllowed = runProbe(added, port) == 0;
        const DWORD rejected = appFirewall::replace(engine, key, { { L"relative.exe", false } }, 0, filters);
        const bool rollbackProtected = rejected != ERROR_SUCCESS && runProbe(selected, port) == 2;
        fs::remove(added);
        status = appFirewall::replace(engine, key, { { added.wstring(), false } }, 0, filters);
        fs::copy_file(executable, added);
        const bool recreatedFileBlocked = status == ERROR_SUCCESS && runProbe(added, port) == 2;
        DWORD index = 0;
        NET_LUID luid { };
        const bool interfaceFound = GetBestInterface(0x01010101, &index) == NO_ERROR
                && ConvertInterfaceIndexToLuid(index, &luid) == NO_ERROR;
        status = appFirewall::replace(engine, key, { { folder.wstring(), true } }, luid.Value, filters);
        const bool interfaceAllowed = interfaceFound && status == ERROR_SUCCESS && runProbe(selected, port) == 0;
        status = appFirewall::replace(engine, key, { { folder.wstring(), true } }, ~luid.Value, filters);
        const bool wrongInterfaceBlocked = status == ERROR_SUCCESS && runProbe(selected, port) == 2;
        status = appFirewall::replace(engine, key, { }, 0, filters);
        const bool released = status == ERROR_SUCCESS && filters.empty() && runProbe(selected, port) == 0;
        passed = baseline && folderBlocked && siblingAllowed && newFileBlocked && exactBlocked && otherFileAllowed
                && rollbackProtected && recreatedFileBlocked && interfaceAllowed && wrongInterfaceBlocked && released;
        std::cout << "baseline=" << baseline << " folderBlocked=" << folderBlocked << " siblingAllowed=" << siblingAllowed
                  << " newFileBlocked=" << newFileBlocked << " exactBlocked=" << exactBlocked
                  << " otherFileAllowed=" << otherFileAllowed << " rollbackProtected=" << rollbackProtected
                  << " recreatedFileBlocked=" << recreatedFileBlocked << " interfaceAllowed=" << interfaceAllowed
                  << " wrongInterfaceBlocked=" << wrongInterfaceBlocked << " released=" << released << '\n';
        fs::remove(added);
        FwpmEngineClose0(engine);
    }
    fs::remove(selected);
    fs::remove(direct);
    fs::remove(selected.parent_path());
    fs::remove(selected.parent_path().parent_path());
    fs::remove(direct.parent_path());
    fs::remove(root);
    std::cout << "WFP status=" << status << '\n';
    return passed ? 0 : 1;
}
