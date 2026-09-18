#ifndef MODULE_CONTAINER_H
#define MODULE_CONTAINER_H

#include "load_status.h"
#include "module_descriptor.h"
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace LogosCore {

// Abstract interface for module isolation / execution environments.
// A container decides *where* and *how* a module process runs (subprocess,
// docker, in-process, etc.) but knows nothing about the module type.
class ModuleContainer {
public:
    virtual ~ModuleContainer() = default;

    virtual std::string id() const = 0;

    virtual bool canHandle(const ModuleDescriptor& desc) const = 0;

    // Launch a module inside this container.  The caller (CompositeModuleLoader)
    // supplies the resolved host binary and CLI arguments — those come from
    // the ModuleLoader, not the container.
    virtual bool launch(const ModuleDescriptor& desc,
                        const std::string& hostBinary,
                        const std::vector<std::string>& args,
                        std::function<void(const std::string& name)> onTerminated,
                        LoadedModuleHandle& out) = 0;

    virtual bool sendToken(const std::string& name, const std::string& token) = 0;

    // Wait, bounded, for the child to report whether its plugin actually
    // loaded. Containers without child observation return Unknown.
    virtual LoadOutcome awaitLoad(const std::string& /*name*/,
                                  std::chrono::milliseconds /*timeout*/)
    {
        return {};
    }

    virtual void terminate(const std::string& name) = 0;

    virtual void terminateAll() = 0;

    virtual bool hasModule(const std::string& name) const = 0;

    virtual std::optional<int64_t> pid(const std::string& /*name*/) const { return std::nullopt; }

    virtual std::unordered_map<std::string, int64_t> getAllPids() const { return {}; }
};

// Optional extension for containers that can keep more than one runtime for
// the same logical module. It deliberately leaves ModuleContainer unchanged:
// existing container binaries and name-only callers continue to use the
// default instance, while a host must explicitly select this extension before
// launching a scoped instance.
//
// Implementations key process state, token delivery, callbacks, liveness, and
// PID lookup by the full ModuleAddress. They must reject invalid addresses and
// a launch that would bind an endpoint already assigned to a different address.
class InstanceAwareModuleContainer : public ModuleContainer {
public:
    virtual bool launchInstance(
        const ModuleDescriptor& desc,
        const std::string& hostBinary,
        const std::vector<std::string>& args,
        std::function<void(const ModuleAddress& address)> onTerminated,
        LoadedModuleHandle& out) = 0;

    virtual bool sendTokenToInstance(const ModuleAddress& address,
                                     const std::string& token) = 0;

    virtual bool terminateInstance(const ModuleAddress& address) = 0;

    virtual bool hasInstance(const ModuleAddress& address) const = 0;

    virtual std::optional<int64_t> instancePid(const ModuleAddress& address) const = 0;

    virtual std::unordered_map<ModuleAddress, int64_t, ModuleAddressHash>
    getAllInstancePids() const = 0;
};

} // namespace LogosCore

#endif // MODULE_CONTAINER_H
