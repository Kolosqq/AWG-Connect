#include "windowsUpdatePackage.h"

#include <array>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <vector>

// clang-format off
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <bcrypt.h>
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
    if (!verifySignature(path, path + L".sig", AWG_UPDATE_PUBLIC_KEY)) {
        return false;
    }
    MSIHANDLE database = 0;
    if (MsiOpenDatabaseW(path.c_str(), nullptr, &database) != ERROR_SUCCESS) {
        return false;
    }
    const auto family = property(database, L"SELECT `Value` FROM `Property` WHERE `Property` = 'UpgradeCode'");
    const auto packageVersion = property(database, L"SELECT `Value` FROM `Property` WHERE `Property` = 'ProductVersion'");
    MsiCloseHandle(database);
    return _wcsicmp(family.c_str(), upgradeCode) == 0 && packageVersion == version;
}

bool windowsUpdate::verifySignature(const std::wstring &path, const std::wstring &signaturePath,
                                    const std::string &publicKeyHex)
{
    constexpr size_t signatureSize = 384;
    constexpr size_t keySize = sizeof(BCRYPT_RSAKEY_BLOB) + 3 + signatureSize;
    if (publicKeyHex.size() != keySize * 2) {
        return false;
    }
    auto nibble = [](char digit) -> int {
        if (digit >= '0' && digit <= '9') {
            return digit - '0';
        }
        return digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 : -1;
    };
    std::array<UCHAR, keySize> publicKey { };
    for (size_t i = 0; i < publicKey.size(); ++i) {
        const auto hi = nibble(publicKeyHex[2 * i]);
        const auto lo = nibble(publicKeyHex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        publicKey[i] = static_cast<UCHAR>(hi * 16 + lo);
    }

    std::ifstream signatureFile(std::filesystem::path(signaturePath), std::ios::binary);
    std::array<UCHAR, signatureSize> signature { };
    if (!signatureFile.read(reinterpret_cast<char *>(signature.data()), signature.size())
        || signatureFile.peek() != std::char_traits<char>::eof()) {
        return false;
    }

    std::ifstream package(std::filesystem::path(path), std::ios::binary);
    if (!package) {
        return false;
    }

    BCRYPT_ALG_HANDLE sha = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    BCRYPT_ALG_HANDLE rsa = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    const bool verified = [&] {
        if (BCryptOpenAlgorithmProvider(&sha, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0
            || BCryptCreateHash(sha, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
            return false;
        }
        std::vector<UCHAR> buffer(64 * 1024);
        size_t size = 0;
        while (package) {
            package.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
            const auto count = static_cast<ULONG>(package.gcount());
            size += count;
            if (size > 512 * 1024 * 1024 || (count && BCryptHashData(hash, buffer.data(), count, 0) != 0)) {
                return false;
            }
        }
        std::array<UCHAR, 32> digest { };
        if (!package.eof() || package.bad() || size == 0 || BCryptFinishHash(hash, digest.data(), digest.size(), 0) != 0
            || BCryptOpenAlgorithmProvider(&rsa, BCRYPT_RSA_ALGORITHM, nullptr, 0) != 0
            || BCryptImportKeyPair(rsa, nullptr, BCRYPT_RSAPUBLIC_BLOB, &key, publicKey.data(), publicKey.size(), 0)
                    != 0) {
            return false;
        }
        BCRYPT_PSS_PADDING_INFO padding { BCRYPT_SHA256_ALGORITHM, 32 };
        return BCryptVerifySignature(key, &padding, digest.data(), digest.size(), signature.data(), signature.size(),
                                     BCRYPT_PAD_PSS)
                == 0;
    }();
    if (key) {
        BCryptDestroyKey(key);
    }
    if (rsa) {
        BCryptCloseAlgorithmProvider(rsa, 0);
    }
    if (hash) {
        BCryptDestroyHash(hash);
    }
    if (sha) {
        BCryptCloseAlgorithmProvider(sha, 0);
    }
    return verified;
}
