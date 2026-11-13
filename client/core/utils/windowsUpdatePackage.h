#pragma once

#include <string>

namespace windowsUpdate
{
    bool hasInstalledProduct(const std::wstring &version);
    bool verifySignature(const std::wstring &path, const std::wstring &signaturePath, const std::string &publicKeyHex);
    bool verifyPackage(const std::wstring &path, const std::wstring &version);
}
