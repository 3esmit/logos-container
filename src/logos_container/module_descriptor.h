#ifndef MODULE_DESCRIPTOR_H
#define MODULE_DESCRIPTOR_H

#include <any>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Qt-free value types passed across the container boundary. A ModuleContainer
// receives a ModuleDescriptor describing what to launch and fills in a
// LoadedModuleHandle on success. These are the only types the container
// contract needs to know about — the ModuleLoader / ModuleFormatLoader
// strategy interfaces that also consume them live in the core (liblogos).

namespace LogosCore {

// Complete identity of one loaded runtime. `moduleName` identifies the
// package definition and `instanceId` identifies one independently managed
// process. An empty instance ID is the legacy/default instance.
struct ModuleAddress {
    static constexpr std::size_t kMaxModuleNameBytes = 64;
    // A scoped Indexer address commonly includes a 64-hex-character Channel
    // ID plus its network scope, so it needs more room than a module name.
    static constexpr std::size_t kMaxInstanceIdBytes = 128;

    std::string moduleName;
    std::string instanceId;

    bool isDefaultInstance() const noexcept
    {
        return instanceId.empty();
    }

    bool isValid() const noexcept
    {
        return isValidRequiredSegment(moduleName, kMaxModuleNameBytes)
            && (instanceId.empty()
                || isValidRequiredSegment(instanceId, kMaxInstanceIdBytes));
    }

    friend bool operator==(const ModuleAddress& lhs, const ModuleAddress& rhs) noexcept
    {
        return lhs.moduleName == rhs.moduleName && lhs.instanceId == rhs.instanceId;
    }

    friend bool operator!=(const ModuleAddress& lhs, const ModuleAddress& rhs) noexcept
    {
        return !(lhs == rhs);
    }

private:
    static bool isValidRequiredSegment(const std::string& value,
                                       std::size_t maxBytes) noexcept
    {
        if (value.empty() || value.size() > maxBytes) return false;

        for (const unsigned char character : value) {
            const bool isLowercase = character >= 'a' && character <= 'z';
            const bool isUppercase = character >= 'A' && character <= 'Z';
            const bool isDigit = character >= '0' && character <= '9';
            if (!isLowercase && !isUppercase && !isDigit
                && character != '_' && character != '-') {
                return false;
            }
        }
        return true;
    }
};

struct ModuleAddressHash {
    std::size_t operator()(const ModuleAddress& address) const noexcept
    {
        const std::size_t moduleHash = std::hash<std::string>{}(address.moduleName);
        const std::size_t instanceHash = std::hash<std::string>{}(address.instanceId);
        return moduleHash ^ (instanceHash + 0x9e3779b9U + (moduleHash << 6U)
                             + (moduleHash >> 2U));
    }
};

// Describes a module the core wants to load/launch.
struct ModuleDescriptor {
    std::string name;
    std::string path;                      // path to the module binary/bundle/wasm/etc.
    std::string format;                    // "qt-plugin", "wasm", "" (empty = default)
    std::vector<std::string> dependencies;
    std::string instancePersistencePath;   // empty if not configured
    std::vector<std::string> modulesDirs;  // directories siblings are looked up in
    nlohmann::json rawMetadata;            // metadata parsed from manifest.json
    nlohmann::json loaderConfig;           // optional: {"id":"docker","image":"..."}, etc.

    // Per-module transport set, serialized as JSON (see
    // logos-cpp-sdk/cpp/logos_transport_config_json.h for the wire
    // shape). Empty = inherit the global default (LocalSocket only).
    // Threaded through to the child subprocess by ModuleLoader
    // implementations so its LogosAPIProvider binds every transport
    // in the set rather than only the global default.
    std::string transportSetJson;

    // Empty selects the legacy/default instance. Appended to preserve the
    // field order used by existing aggregate initialization.
    std::string instanceId;

    ModuleAddress address() const
    {
        return {name, instanceId};
    }
};

// A handle to a successfully loaded module. Stored in ModuleRegistry (ModuleInfo).
struct LoadedModuleHandle {
    std::string name;
    int64_t pid = -1;      // -1 when not process-based (in-proc, wasm, remote, etc.)
    std::string endpoint;  // transport-specific URI, e.g. "qtro+unix://my_module"
    std::any opaque;       // loader-private state (optional)

    // Appended to preserve the field order used by existing aggregate
    // initialization. Empty means the legacy/default instance.
    std::string instanceId;

    ModuleAddress address() const
    {
        return {name, instanceId};
    }
};

} // namespace LogosCore

#endif // MODULE_DESCRIPTOR_H
