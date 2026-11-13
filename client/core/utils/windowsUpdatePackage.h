#pragma once

#include <string>

namespace windowsUpdate
{
    bool hasInstalledProduct(const std::wstring &version);
    bool verifyPackage(const std::wstring &path, const std::wstring &version);
}
