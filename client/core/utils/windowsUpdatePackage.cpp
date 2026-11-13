#include "windowsUpdatePackage.h"

#include <array>
#include <cwchar>
#include <cwctype>

// clang-format off
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
// clang-format on

#include "version.h"

namespace
{
    constexpr wchar_t upgradeCode[] = L"{A7E8403F-F7E7-46BB-A631-6D5518DB7C05}";

    std::wstring property(MSIHANDLE database, const wchar_t *query)
    {
        MSIHANDLE view = 0;
        MSIHANDLE record = 0;
        std::wstring result;
        if (MsiDatabaseOpenViewW(database, query, &view) == ERROR_SUCCESS && MsiViewExecute(view, 0) == ERROR_SUCCESS
            && MsiViewFetch(view, &record) == ERROR_SUCCESS) {
            wchar_t value[256] { };
            DWORD length = 256;
            if (MsiRecordGetStringW(record, 1, value, &length) == ERROR_SUCCESS) {
                result.assign(value, length);
            }
        }
        if (record) {
            MsiCloseHandle(record);
        }
        if (view) {
            MsiCloseHandle(view);
        }
        return result;
    }
}

bool windowsUpdate::hasInstalledProduct(const std::wstring &version)
{
    wchar_t product[39] { };
    for (DWORD i = 0; MsiEnumRelatedProductsW(upgradeCode, 0, i, product) == ERROR_SUCCESS; ++i) {
        wchar_t installed[256] { };
        DWORD size = 256;
        if (MsiQueryProductStateW(product) == INSTALLSTATE_DEFAULT
            && MsiGetProductInfoW(product, L"VersionString", installed, &size) == ERROR_SUCCESS && version == installed) {
            return true;
        }
    }
    return false;
}

bool windowsUpdate::verifyPackage(const std::wstring &path, const std::wstring &version)
{
    const std::string pin = AWG_UPDATE_SIGNER_SHA256;
    if (pin.size() != 64) {
        return false;
    }

    WINTRUST_FILE_INFO file { };
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = path.c_str();
    WINTRUST_DATA trust { };
    trust.cbStruct = sizeof(trust);
    trust.dwUIChoice = WTD_UI_NONE;
    trust.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    trust.dwUnionChoice = WTD_CHOICE_FILE;
    trust.pFile = &file;
    trust.dwStateAction = WTD_STATEACTION_VERIFY;
    trust.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    bool verified = false;
    if (WinVerifyTrust(nullptr, &policy, &trust) == ERROR_SUCCESS) {
        const auto data = WTHelperProvDataFromStateData(trust.hWVTStateData);
        const auto signer = data ? WTHelperGetProvSignerFromChain(data, 0, FALSE, 0) : nullptr;
        const auto cert = signer ? WTHelperGetProvCertFromChain(signer, 0) : nullptr;
        std::array<BYTE, 32> fingerprint { };
        DWORD length = static_cast<DWORD>(fingerprint.size());
        if (cert && CertGetCertificateContextProperty(cert->pCert, CERT_SHA256_HASH_PROP_ID, fingerprint.data(), &length)
            && length == fingerprint.size()) {
            constexpr char hex[] = "0123456789abcdef";
            verified = true;
            for (size_t i = 0; i < fingerprint.size(); ++i) {
                if (std::towlower(pin[2 * i]) != hex[fingerprint[i] >> 4]
                    || std::towlower(pin[2 * i + 1]) != hex[fingerprint[i] & 15]) {
                    verified = false;
                    break;
                }
            }
        }
    }
    trust.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policy, &trust);

    MSIHANDLE database = 0;
    if (!verified || MsiOpenDatabaseW(path.c_str(), nullptr, &database) != ERROR_SUCCESS) {
        return false;
    }
    const auto family = property(database, L"SELECT `Value` FROM `Property` WHERE `Property` = 'UpgradeCode'");
    const auto packageVersion = property(database, L"SELECT `Value` FROM `Property` WHERE `Property` = 'ProductVersion'");
    MsiCloseHandle(database);
    return _wcsicmp(family.c_str(), upgradeCode) == 0 && packageVersion == version;
}
