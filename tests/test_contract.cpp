// Self-contained tests for the logos-container contract: the
// ModuleDescriptor/LoadedModuleHandle value types and the ModuleContainer
// interface. These are the pieces every container implementation and liblogos
// itself depend on, so a regression here breaks the whole stack.
#include <gtest/gtest.h>

#include <logos_container/module_descriptor.h>
#include <logos_container/module_container.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Value types compile and carry their fields
// ---------------------------------------------------------------------------

TEST(ModuleDescriptor, DefaultsAndAssignment) {
    LogosCore::ModuleDescriptor desc;
    desc.name = "m";
    desc.instanceId = "zone_a";
    desc.format = "qt-plugin";
    desc.rawMetadata = nlohmann::json::object();
    EXPECT_EQ(desc.name, "m");
    EXPECT_EQ(desc.address(), (LogosCore::ModuleAddress{"m", "zone_a"}));
    EXPECT_EQ(desc.format, "qt-plugin");

    LogosCore::LoadedModuleHandle handle;
    EXPECT_EQ(handle.pid, -1);
    handle.name = "m";
    handle.instanceId = "zone_a";
    EXPECT_EQ(handle.name, "m");
    EXPECT_EQ(handle.address(), (LogosCore::ModuleAddress{"m", "zone_a"}));
}

TEST(ModuleDescriptor, LegacyAggregateInitializationKeepsDefaultInstance) {
    const LogosCore::ModuleDescriptor descriptor{
        "storage_module", "/module/storage", "qt-plugin", {}, {}, {},
        nlohmann::json::object(), nlohmann::json::object(), "{}"};

    EXPECT_EQ(descriptor.name, "storage_module");
    EXPECT_TRUE(descriptor.address().isValid());
    EXPECT_TRUE(descriptor.address().isDefaultInstance());
}

TEST(ModuleAddress, ValidatesLogicalAndRuntimeIdentity) {
    EXPECT_TRUE((LogosCore::ModuleAddress{"lez_indexer_module", {}}).isValid());
    EXPECT_TRUE((LogosCore::ModuleAddress{"lez_indexer_module", "zone_0101"}).isValid());
    EXPECT_TRUE((LogosCore::ModuleAddress{
        "lez_indexer_module", "testnet_" + std::string(64, '0')}).isValid());
    EXPECT_FALSE((LogosCore::ModuleAddress{{}, "zone_0101"}).isValid());
    EXPECT_FALSE((LogosCore::ModuleAddress{"lez/indexer", "zone_0101"}).isValid());
    EXPECT_FALSE((LogosCore::ModuleAddress{"lez_indexer_module", "zone/0101"}).isValid());
    EXPECT_FALSE((LogosCore::ModuleAddress{"lez_indexer_module", "zone.0101"}).isValid());
    EXPECT_FALSE((LogosCore::ModuleAddress{
        "lez_indexer_module", std::string(129, 'a')}).isValid());
}

// A minimal ModuleContainer implementation must compile against the interface.
namespace {
class NullContainer : public LogosCore::ModuleContainer {
public:
    std::string id() const override { return "null"; }
    bool canHandle(const LogosCore::ModuleDescriptor&) const override { return false; }
    bool launch(const LogosCore::ModuleDescriptor&, const std::string&,
                const std::vector<std::string>&,
                std::function<void(const std::string&)>,
                LogosCore::LoadedModuleHandle&) override { return false; }
    bool sendToken(const std::string&, const std::string&) override { return false; }
    void terminate(const std::string&) override {}
    void terminateAll() override {}
    bool hasModule(const std::string&) const override { return false; }
};

class LegacyContainer : public LogosCore::ModuleContainer {
public:
    std::string id() const override { return "legacy"; }
    bool canHandle(const LogosCore::ModuleDescriptor&) const override { return true; }
    bool launch(const LogosCore::ModuleDescriptor& desc, const std::string&,
                const std::vector<std::string>&,
                std::function<void(const std::string&)> onTerminated,
                LogosCore::LoadedModuleHandle& out) override
    {
        liveModules.emplace(desc.name, std::move(onTerminated));
        out.name = desc.name;
        out.pid = 42;
        return true;
    }
    bool sendToken(const std::string& name, const std::string& token) override
    {
        if (!hasModule(name)) return false;
        tokens[name] = token;
        return true;
    }
    void terminate(const std::string& name) override
    {
        auto entry = liveModules.find(name);
        if (entry == liveModules.end()) return;
        const auto callback = std::move(entry->second);
        liveModules.erase(entry);
        if (callback) callback(name);
    }
    void terminateAll() override { liveModules.clear(); }
    bool hasModule(const std::string& name) const override
    {
        return liveModules.find(name) != liveModules.end();
    }
    std::optional<int64_t> pid(const std::string& name) const override
    {
        return hasModule(name) ? std::optional<int64_t>{42} : std::nullopt;
    }

private:
    std::unordered_map<std::string, std::function<void(const std::string&)>> liveModules;
    std::unordered_map<std::string, std::string> tokens;
};

class ScopedContainer : public LogosCore::InstanceAwareModuleContainer {
public:
    std::string id() const override { return "scoped"; }
    bool canHandle(const LogosCore::ModuleDescriptor&) const override { return true; }
    bool launch(const LogosCore::ModuleDescriptor&, const std::string&,
                const std::vector<std::string>&,
                std::function<void(const std::string&)>,
                LogosCore::LoadedModuleHandle&) override { return false; }
    bool sendToken(const std::string&, const std::string&) override { return false; }
    void terminate(const std::string&) override {}
    void terminateAll() override { liveInstances.clear(); }
    bool hasModule(const std::string&) const override { return false; }

    bool launchInstance(const LogosCore::ModuleDescriptor& desc, const std::string&,
                        const std::vector<std::string>&,
                        std::function<void(const LogosCore::ModuleAddress&)> onTerminated,
                        LogosCore::LoadedModuleHandle& out) override
    {
        const LogosCore::ModuleAddress address = desc.address();
        if (!address.isValid() || hasInstance(address)) return false;
        const int64_t processId = nextPid++;
        liveInstances.emplace(address, Entry{std::move(onTerminated), processId});
        out.name = address.moduleName;
        out.instanceId = address.instanceId;
        out.pid = processId;
        return true;
    }
    bool sendTokenToInstance(const LogosCore::ModuleAddress& address,
                             const std::string&) override
    {
        return hasInstance(address);
    }
    bool terminateInstance(const LogosCore::ModuleAddress& address) override
    {
        const auto entry = liveInstances.find(address);
        if (entry == liveInstances.end()) return false;
        const auto callback = std::move(entry->second.onTerminated);
        liveInstances.erase(entry);
        if (callback) callback(address);
        return true;
    }
    bool hasInstance(const LogosCore::ModuleAddress& address) const override
    {
        return liveInstances.find(address) != liveInstances.end();
    }
    std::optional<int64_t> instancePid(const LogosCore::ModuleAddress& address) const override
    {
        const auto entry = liveInstances.find(address);
        if (entry == liveInstances.end()) return std::nullopt;
        return entry->second.processId;
    }
    std::unordered_map<LogosCore::ModuleAddress, int64_t, LogosCore::ModuleAddressHash>
    getAllInstancePids() const override
    {
        std::unordered_map<LogosCore::ModuleAddress, int64_t, LogosCore::ModuleAddressHash> result;
        for (const auto& [address, entry] : liveInstances) {
            result.emplace(address, entry.processId);
        }
        return result;
    }

private:
    struct Entry {
        std::function<void(const LogosCore::ModuleAddress&)> onTerminated;
        int64_t processId;
    };

    int64_t nextPid = 99;
    std::unordered_map<LogosCore::ModuleAddress, Entry, LogosCore::ModuleAddressHash>
        liveInstances;
};
} // namespace

TEST(ModuleContainer, InterfaceIsImplementable) {
    NullContainer c;
    EXPECT_EQ(c.id(), "null");
    EXPECT_FALSE(c.hasModule("x"));
    EXPECT_FALSE(c.pid("x").has_value());        // default impl
    EXPECT_TRUE(c.getAllPids().empty());          // default impl
}

TEST(ModuleContainer, LegacyContainerKeepsNameOnlyContract) {
    LegacyContainer container;
    LogosCore::ModuleDescriptor descriptor;
    descriptor.name = "storage_module";
    LogosCore::LoadedModuleHandle handle;
    std::string terminated;

    ASSERT_TRUE(container.launch(
        descriptor, "host", {},
        [&terminated](const std::string& name) { terminated = name; },
        handle));
    EXPECT_TRUE(container.hasModule(descriptor.name));
    EXPECT_TRUE(container.sendToken(descriptor.name, "token"));
    EXPECT_EQ(container.pid(descriptor.name), std::optional<int64_t>{42});
    container.terminate(descriptor.name);
    EXPECT_EQ(terminated, descriptor.name);
}

TEST(ModuleContainer, ScopedInstancesRemainIndependent) {
    ScopedContainer container;
    LogosCore::ModuleDescriptor lez;
    lez.name = "lez_indexer_module";
    lez.instanceId = "zone_0101";
    LogosCore::ModuleDescriptor paradox = lez;
    paradox.instanceId = "zone_8888";
    LogosCore::LoadedModuleHandle lezHandle;
    LogosCore::LoadedModuleHandle paradoxHandle;
    std::vector<LogosCore::ModuleAddress> terminated;

    ASSERT_TRUE(container.launchInstance(
        lez, "host", {},
        [&terminated](const LogosCore::ModuleAddress& address) { terminated.push_back(address); },
        lezHandle));
    ASSERT_TRUE(container.launchInstance(
        paradox, "host", {},
        [&terminated](const LogosCore::ModuleAddress& address) { terminated.push_back(address); },
        paradoxHandle));
    EXPECT_TRUE(container.sendTokenToInstance(lez.address(), "lez-token"));
    EXPECT_TRUE(container.sendTokenToInstance(paradox.address(), "paradox-token"));
    EXPECT_TRUE(container.hasInstance(lez.address()));
    EXPECT_TRUE(container.hasInstance(paradox.address()));
    EXPECT_NE(lezHandle.address(), paradoxHandle.address());
    EXPECT_NE(container.instancePid(lez.address()), container.instancePid(paradox.address()));
    EXPECT_EQ(container.getAllInstancePids().size(), 2U);

    ASSERT_TRUE(container.terminateInstance(lez.address()));
    EXPECT_FALSE(container.hasInstance(lez.address()));
    EXPECT_TRUE(container.hasInstance(paradox.address()));
    ASSERT_EQ(terminated.size(), 1U);
    EXPECT_EQ(terminated.front(), lez.address());
}
