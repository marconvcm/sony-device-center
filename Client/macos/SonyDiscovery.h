#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

// Discovery identifies candidates, not confirmed protocol support. Cached Sony
// service records also admit renamed headsets without opening Bluetooth links.
inline bool isSonyHeadsetCandidate(std::string_view name, bool hasSonyService)
{
    if (hasSonyService) return true;
    std::string upper(name);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    std::string_view model(upper);
    while (!model.empty() && std::isspace(static_cast<unsigned char>(model.front())))
        model.remove_prefix(1);
    if (model.substr(0, 5) == "SONY ") model.remove_prefix(5);
    // Some paired entries use the LE_ alias of the headset's model name.
    if (model.substr(0, 3) == "LE_") model.remove_prefix(3);
    for (std::string_view prefix : {"WH-", "WF-", "WI-", "MDR-"}) {
        if (model.substr(0, prefix.size()) == prefix && model.size() > prefix.size()) return true;
    }
    for (std::string_view family : {"LINKBUDS", "ULT WEAR"}) {
        if (model.substr(0, family.size()) == family && (model.size() == family.size()
            || model[family.size()] == ' ' || model[family.size()] == '-')) return true;
    }
    return false;
}
