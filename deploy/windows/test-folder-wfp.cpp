// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <initguid.h>
#include <fwpmu.h>
#include <rpc.h>
// clang-format on

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int probe(unsigned short port)
{
    const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in target { };
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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
    const fs::path root = fs::temp_directory_path() / (L"awg-wfp-test-" + std::to_wstring(GetCurrentProcessId()));
    const fs::path selected = root / L"selected" / L"nested" / L"probe.exe";
    const fs::path direct = root / L"selected-other" / L"probe.exe";
    fs::create_directories(selected.parent_path());
    fs::create_directories(direct.parent_path());
    fs::copy_file(executable, selected);
    fs::copy_file(executable, direct);
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address { };
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address));
    listen(listener, SOMAXCONN);
    int addressSize = sizeof(address);
    getsockname(listener, reinterpret_cast<sockaddr *>(&address), &addressSize);
    const auto port = ntohs(address.sin_port);
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
        FWP_BYTE_BLOB *appId = nullptr;
        if (status == ERROR_SUCCESS) {
            status = FwpmGetAppIdFromFileName0(selected.c_str(), &appId);
        }
        if (status == ERROR_SUCCESS) {
            std::wstring lower(reinterpret_cast<wchar_t *>(appId->data));
            lower.resize(lower.size() - std::wstring(L"nested\\probe.exe").size());
            std::wstring upper = lower;
            ++upper.back();
            FWP_BYTE_BLOB lowBlob { static_cast<UINT32>((lower.size() + 1) * sizeof(wchar_t)),
                                    reinterpret_cast<UINT8 *>(lower.data()) };
            FWP_BYTE_BLOB highBlob { static_cast<UINT32>((upper.size() + 1) * sizeof(wchar_t)),
                                     reinterpret_cast<UINT8 *>(upper.data()) };
            FWP_RANGE0 range { };
            range.valueLow.type = FWP_BYTE_BLOB_TYPE;
            range.valueLow.byteBlob = &lowBlob;
            range.valueHigh.type = FWP_BYTE_BLOB_TYPE;
            range.valueHigh.byteBlob = &highBlob;
            FWPM_FILTER_CONDITION0 condition { };
            condition.fieldKey = FWPM_CONDITION_ALE_APP_ID;
            condition.matchType = FWP_MATCH_RANGE;
            condition.conditionValue.type = FWP_RANGE_TYPE;
            condition.conditionValue.rangeValue = &range;
            FWPM_FILTER0 filter { };
            filter.subLayerKey = key;
            filter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4;
            filter.displayData.name = const_cast<wchar_t *>(L"AWG isolated folder block");
            filter.action.type = FWP_ACTION_BLOCK;
            filter.weight.type = FWP_UINT8;
            filter.weight.uint8 = 15;
            filter.numFilterConditions = 1;
            filter.filterCondition = &condition;
            const bool baseline = runProbe(selected, port) == 0 && runProbe(direct, port) == 0;
            UINT64 rangeFilter = 0;
            status = FwpmFilterAdd0(engine, &filter, nullptr, &rangeFilter);
            if (status == ERROR_SUCCESS) {
                const bool blocked = runProbe(selected, port) == 2;
                const bool allowed = runProbe(direct, port) == 0;
                std::cout << "baseline=" << baseline << " folderBlocked=" << blocked << " siblingAllowed=" << allowed
                          << '\n';
                FwpmFilterDeleteById0(engine, rangeFilter);
                condition.matchType = FWP_MATCH_EQUAL;
                condition.conditionValue.type = FWP_BYTE_BLOB_TYPE;
                condition.conditionValue.byteBlob = appId;
                const DWORD exactStatus = FwpmFilterAdd0(engine, &filter, nullptr, nullptr);
                const bool exactBlocked = exactStatus == ERROR_SUCCESS && runProbe(selected, port) == 2;
                const bool exactDirect = runProbe(direct, port) == 0;
                passed = baseline && exactBlocked && exactDirect;
                std::cout << "exactStatus=" << exactStatus << " exactBlocked=" << exactBlocked
                          << " exactSiblingAllowed=" << exactDirect << '\n';
            }
            FwpmFreeMemory0(reinterpret_cast<void **>(&appId));
        }
        FwpmEngineClose0(engine);
    }
    closesocket(listener);
    fs::remove(selected);
    fs::remove(direct);
    fs::remove(selected.parent_path());
    fs::remove(selected.parent_path().parent_path());
    fs::remove(direct.parent_path());
    fs::remove(root);
    std::cout << "WFP status=" << status << '\n';
    return passed ? 0 : 1;
}
