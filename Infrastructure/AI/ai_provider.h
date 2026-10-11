#pragma once
#include <atomic>
#include <memory>
#include <string>

struct AIProviderReply
{
    enum class Status { Ok, Timeout, Unavailable } status = Status::Unavailable;
    std::string json;
};
// This interface receives sanitized data only. It has no target HTTP client,
// request budget, credentials, policy mutation, or probe execution capability.
class IAIProvider
{
public:
    virtual ~IAIProvider() = default;
    virtual std::string Name() const = 0;
    virtual bool IsMock() const = 0;
    virtual AIProviderReply Generate(const std::string& contextJson,
        int timeoutMs, const std::shared_ptr<std::atomic_bool>& cancelled) = 0;
};
class MockAIProvider final : public IAIProvider
{
public:
    std::string Name() const override { return "mock-offline-v1"; }
    bool IsMock() const override { return true; }
    AIProviderReply Generate(const std::string& contextJson, int timeoutMs,
        const std::shared_ptr<std::atomic_bool>& cancelled) override;
};
