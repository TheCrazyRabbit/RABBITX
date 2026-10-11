#include "Infrastructure/Security/entitlement_client.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace RabbitXAuth;
struct Store : TokenStore {
    std::string token;
    bool failSave = false;
    std::string Load() override { return token; }
    bool Save(const std::string& value) override { if (failSave) return false; token = value; return true; }
    bool Clear() override { token.clear(); return true; }
};
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    int tests = 0;
    const auto run = [&](const char* name, auto fn) { fn(); ++tests; std::cout << "PASS " << name << '\n'; };
    try {
        run("email challenge and verified session; no state grant from login", [] {
            Store store; std::vector<std::string> paths;
            Client client([&](auto method, auto path, auto body, auto token) {
                Require(method == "POST" && token.empty(), "sign-in transport"); paths.push_back(path);
                if (path == "/v1/auth/start") { Require(body.find("owner@example.com") != std::string::npos, "email"); return Reply{200, std::string(64, 'a')}; }
                Require(body.find("12345678") != std::string::npos, "OTP"); return Reply{200, "", std::string(64, 'b')};
            }, store, [] { return 100; });
            std::string challenge; Require(client.Start("owner@example.com", challenge).ok, "start");
            const auto signedIn = client.Finish(challenge, "12345678");
            Require(signedIn.ok && !signedIn.pro && store.token.size() == 64, "sign-in is not a Pro grant"); Require(paths.size() == 2, "flow");
        });
        run("no session or edited local file cannot grant Pro", [] {
            Store store; bool called = false; Client client([&](auto, auto, auto, auto) { called = true; return Reply{200, "", "", "Pro", "500"}; }, store, [] { return 100; });
            Require(!client.Check().pro && !called, "missing session"); store.token = "Pro forever";
            Require(!client.Check().pro && !called, "local text");
        });
        run("paid status and gate must be refreshed each time", [] {
            Store store; store.token = std::string(64, 'a'); int calls = 0;
            Client client([&](auto method, auto path, auto body, auto token) {
                Require(method == "POST" && path == "/v1/features/pro-foundation/check" && body.empty() && token == store.token, "server gate");
                return ++calls == 1 ? Reply{200, "", "", "Pro", "500"} : Reply{403, "", "", "Free", "0"};
            }, store, [] { return 100; });
            Require(client.Check(true).pro, "first check"); Require(!client.Check(true).pro && calls == 2, "no cached grant");
        });
        run("network loss, service outage and rate limit deny Pro without erasing session", [] {
            Store store; store.token = std::string(64, 'a'); int status = 200;
            Client client([&](auto, auto, auto, auto) { return Reply{status, "", "", "Pro", "500"}; }, store, [] { return 100; });
            Require(client.Check().pro, "online");
            for (const int failure : {0, 503, 429, 500, 302}) { status = failure; Require(!client.Check().pro && !store.token.empty(), "fail closed, retain retry"); }
        });
        run("expiry and malformed authorization headers deny", [] {
            Store store; store.token = std::string(64, 'a'); std::string tier = "Pro", end = "100";
            Client client([&](auto, auto, auto, auto) { return Reply{200, "", "", tier, end}; }, store, [] { return 100; });
            for (const auto& value : {"100", "0", "-1", "abc", "9999999999999999999999"}) { end = value; Require(!client.Check().pro, "bad/expired end"); }
            end = "500"; tier = "pro"; Require(!client.Check().pro, "strict tier");
        });
        run("revoked or expired session removed locally", [] {
            Store store; store.token = std::string(64, 'a'); Client client([](auto, auto, auto, auto) { return Reply{401}; }, store, [] { return 100; });
            Require(!client.Check().pro && store.token.empty(), "revocation");
        });
        run("logout revokes server before clearing; offline logout remains retryable", [] {
            Store store; store.token = std::string(64, 'a'); int status = 0;
            Client client([&](auto method, auto path, auto, auto) { Require(method == "POST" && path == "/v1/auth/logout", "logout endpoint"); return Reply{status}; }, store, [] { return 100; });
            Require(!client.Logout().ok && !store.token.empty(), "offline retry"); status = 200;
            Require(client.Logout().ok && store.token.empty(), "revoked");
        });
        run("invalid input, code rejection, corrupt response and vault failure", [] {
            Store store; Reply reply{200, "bad", "bad"};
            Client client([&](auto, auto, auto, auto) { return reply; }, store, [] { return 100; });
            std::string challenge; Require(!client.Start("bad\ncontrol", challenge).ok, "controls");
            Require(!client.Start("owner@example.com", challenge).ok && challenge.empty(), "bad challenge");
            Require(!client.Finish(std::string(64, 'a'), "bad").ok, "invalid code");
            reply.status = 401; Require(!client.Finish(std::string(64, 'a'), "12345678").ok, "server rejected");
            reply = Reply{200, "", std::string(64, 'a')}; store.failSave = true;
            Require(!client.Finish(std::string(64, 'a'), "12345678").ok && store.token.empty(), "vault failure");
        });
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
    std::cout << tests << " native authorization tests passed\n";
    return 0;
}
