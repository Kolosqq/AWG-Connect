#include "appfirewall.h"

#include <filesystem>
#include <limits>

namespace
{
    DWORD applicationPath(const appFirewall::Selection &selection, std::wstring &path)
    {
        std::filesystem::path existing(selection.path);
        if (!existing.is_absolute()) {
            return ERROR_BAD_PATHNAME;
        }
        while (!existing.has_filename() && existing != existing.root_path()) {
            existing = existing.parent_path();
        }
        std::wstring suffix;
        HANDLE file = INVALID_HANDLE_VALUE;
        while (file == INVALID_HANDLE_VALUE) {
            file = CreateFileW(existing.c_str(), FILE_READ_ATTRIBUTES,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (file != INVALID_HANDLE_VALUE) {
                break;
            }
            const DWORD error = GetLastError();
            if ((error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) || existing == existing.root_path()) {
                return error;
            }
            suffix = L"\\" + existing.filename().wstring() + suffix;
            existing = existing.parent_path();
        }
        wchar_t buffer[32768];
        const DWORD length = GetFinalPathNameByHandleW(file, buffer, 32768, FILE_NAME_NORMALIZED | VOLUME_NAME_NT);
        const DWORD error = length == 0 ? GetLastError() : ERROR_SUCCESS;
        CloseHandle(file);
        if (error != ERROR_SUCCESS || length >= 32768) {
            return error != ERROR_SUCCESS ? error : ERROR_FILENAME_EXCED_RANGE;
        }
        path.assign(buffer, length);
        path += suffix;
        std::wstring lower(path.size(), L'\0');
        if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, path.data(), static_cast<int>(path.size()),
                           lower.data(), static_cast<int>(lower.size()), nullptr, nullptr, 0)) {
            return GetLastError();
        }
        path.swap(lower);
        if (selection.directory && path.back() != L'\\') {
            path.push_back(L'\\');
        }
        return ERROR_SUCCESS;
    }
}

DWORD appFirewall::replace(HANDLE engine, const GUID &sublayer, const std::vector<Selection> &selection,
                           UINT64 allowedInterface, std::vector<UINT64> &activeFilters)
{
    DWORD status = FwpmTransactionBegin0(engine, 0);
    if (status != ERROR_SUCCESS) {
        return status;
    }
    std::vector<UINT64> replacement;
    for (UINT64 id : activeFilters) {
        status = FwpmFilterDeleteById0(engine, id);
        if (status != ERROR_SUCCESS && status != FWP_E_FILTER_NOT_FOUND) {
            FwpmTransactionAbort0(engine);
            return status;
        }
    }
    for (const auto &entry : selection) {
        std::wstring lower;
        status = applicationPath(entry, lower);
        if (status != ERROR_SUCCESS) {
            FwpmTransactionAbort0(engine);
            return status;
        }
        std::wstring upper = lower;
        ++upper.back();
        FWP_BYTE_BLOB low { static_cast<UINT32>((lower.size() + 1) * sizeof(wchar_t)),
                            reinterpret_cast<UINT8 *>(lower.data()) };
        FWP_BYTE_BLOB high { static_cast<UINT32>((upper.size() + 1) * sizeof(wchar_t)),
                             reinterpret_cast<UINT8 *>(upper.data()) };
        FWP_RANGE0 range { };
        range.valueLow.type = FWP_BYTE_BLOB_TYPE;
        range.valueLow.byteBlob = &low;
        range.valueHigh.type = FWP_BYTE_BLOB_TYPE;
        range.valueHigh.byteBlob = &high;
        FWPM_FILTER_CONDITION0 conditions[2] { };
        conditions[0].fieldKey = FWPM_CONDITION_ALE_APP_ID;
        conditions[0].matchType = entry.directory ? FWP_MATCH_RANGE : FWP_MATCH_EQUAL;
        conditions[0].conditionValue.type = entry.directory ? FWP_RANGE_TYPE : FWP_BYTE_BLOB_TYPE;
        if (entry.directory) {
            conditions[0].conditionValue.rangeValue = &range;
        } else {
            conditions[0].conditionValue.byteBlob = &low;
        }
        conditions[1].fieldKey = FWPM_CONDITION_IP_LOCAL_INTERFACE;
        conditions[1].matchType = FWP_MATCH_NOT_EQUAL;
        conditions[1].conditionValue.type = FWP_UINT64;
        conditions[1].conditionValue.uint64 = &allowedInterface;
        UINT64 weight = (std::numeric_limits<UINT64>::max)();
        FWPM_FILTER0 filter { };
        filter.subLayerKey = sublayer;
        filter.displayData.name = const_cast<wchar_t *>(L"AWG selected application protection");
        filter.action.type = FWP_ACTION_BLOCK;
        filter.weight.type = FWP_UINT64;
        filter.weight.uint64 = &weight;
        filter.numFilterConditions = allowedInterface == 0 ? 1 : 2;
        filter.filterCondition = conditions;
        for (const GUID &layer : { FWPM_LAYER_ALE_AUTH_CONNECT_V4, FWPM_LAYER_ALE_AUTH_CONNECT_V6,
                                   FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6 }) {
            filter.layerKey = layer;
            UINT64 id = 0;
            status = FwpmFilterAdd0(engine, &filter, nullptr, &id);
            if (status != ERROR_SUCCESS) {
                FwpmTransactionAbort0(engine);
                return status;
            }
            replacement.push_back(id);
        }
    }
    status = FwpmTransactionCommit0(engine);
    if (status == ERROR_SUCCESS) {
        activeFilters.swap(replacement);
    } else {
        FwpmTransactionAbort0(engine);
    }
    return status;
}
