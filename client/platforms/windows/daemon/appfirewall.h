#ifndef APPFIREWALL_H
#define APPFIREWALL_H

// clang-format off
#include <windows.h>
#include <fwpmu.h>
// clang-format on

#include <string>
#include <vector>

namespace appFirewall
{
    struct Selection
    {
        std::wstring path;
        bool directory = false;
    };

    DWORD replace(HANDLE engine, const GUID &sublayer, const std::vector<Selection> &selection, UINT64 allowedInterface,
                  std::vector<UINT64> &activeFilters);
}

#endif
