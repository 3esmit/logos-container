#ifndef LOAD_STATUS_H
#define LOAD_STATUS_H

#include <string>

namespace LogosCore {

// A child reports this line after its plugin has loaded:
//
//     @logos-load-status ok
//     @logos-load-status failed <reason>
//
// Containers that can observe child stdout use this contract in awaitLoad().
// Containers that cannot observe it return Unknown, preserving the original
// launch semantics.
constexpr const char* kLoadStatusPrefix = "@logos-load-status";
constexpr const char* kLoadStatusOk = "ok";
constexpr const char* kLoadStatusFailed = "failed";

enum class LoadVerdict {
    Loaded,
    Failed,
    Unknown,
};

struct LoadOutcome {
    LoadVerdict verdict = LoadVerdict::Unknown;
    std::string reason;
};

} // namespace LogosCore

#endif // LOAD_STATUS_H
