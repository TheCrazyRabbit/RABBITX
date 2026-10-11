#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace RabbitXAuth {
struct Reply {
    int status = 0;
    std::string challenge, session, tier, validUntil;
};
using Transport = std::function<Reply(const std::string&, const std::string&, const std::string&, const std::string&)>;
struct TokenStore {
    virtual ~TokenStore() = default;
    virtual std::string Load() = 0;
    virtual bool Save(const std::string&) = 0;
    virtual bool Clear() = 0;
};
struct Result {
    bool ok = false;
    bool pro = false;
    std::string message;
};
class Client {
public:
    Client(Transport transport, TokenStore& store, std::function<int64_t()> clock)
        : transport_(transport), store_(store), clock_(clock) {}
    Result Start(const std::string& email, std::string& challenge);
    Result Finish(const std::string& challenge, const std::string& code);
    Result Check(bool gate = false);
    Result Logout();
private:
    Transport transport_;
    TokenStore& store_;
    std::function<int64_t()> clock_;
};
// --auth login|status|check|logout; returns -1 for a normal audit command.
int RunCommand(int argc, char* argv[]);
}
