#include "entitlement_client.h"
#include <algorithm>
#include <utility>

namespace RabbitXAuth {
namespace {
bool HexToken(const std::string& token) {
    return token.size() == 64 && std::all_of(token.begin(), token.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
Result Failure(int status) {
    if (status == 401) return {false, false, "Session expired or revoked. Sign in again."};
    if (status == 429) return {false, false, "Too many requests. Try again later."};
    if (status == 503 || status == 404) return {false, false, "Account service unavailable or not enabled. Free features remain available."};
    if (status == 0) return {false, false, "Network unavailable. Pro access cannot be verified; Free features remain available."};
    return {false, false, "Authorization request rejected. Free features remain available."};
}
std::string JsonString(const std::string& text) {
    std::string value = "\"";
    for (unsigned char c : text) {
        if (c < 32) return {}; // Do not place console control characters into requests.
        if (c == '\\' || c == '"') value += '\\';
        value += static_cast<char>(c);
    }
    return value + "\"";
}
}
Result Client::Start(const std::string& email, std::string& challenge) {
    challenge.clear();
    const auto value = JsonString(email);
    if (value.empty() || email.size() > 254) return {false, false, "Enter a valid email."};
    auto reply = transport_("POST", "/v1/auth/start", "{\"email\":" + value + "}", "");
    if (reply.status != 200) return Failure(reply.status);
    if (!HexToken(reply.challenge)) return {false, false, "Invalid account service response."};
    challenge = reply.challenge;
    return {true, false, "Check your email for the 8-digit sign-in code."};
}
Result Client::Finish(const std::string& challenge, const std::string& code) {
    if (!HexToken(challenge) || code.size() != 8 || !std::all_of(code.begin(), code.end(), [](char c) { return c >= '0' && c <= '9'; }))
        return {false, false, "Enter the 8-digit code."};
    auto reply = transport_("POST", "/v1/auth/finish", "{\"challenge_id\":\"" + challenge + "\",\"code\":\"" + code + "\"}", "");
    if (reply.status == 401) return {false, false, "Code invalid, expired or already used. Start sign-in again."};
    if (reply.status != 200) return Failure(reply.status);
    if (!HexToken(reply.session)) return {false, false, "Invalid session response."};
    if (!store_.Save(reply.session)) return {false, false, "Windows could not save the session. No access enabled."};
    return {true, false, "Signed in. Checking your subscription requires an online server response."};
}
Result Client::Check(bool gate) {
    const auto token = store_.Load();
    if (!HexToken(token)) return {false, false, "Free. Use --auth login to sign in."};
    const auto reply = transport_(gate ? "POST" : "GET", gate ? "/v1/features/pro-foundation/check" : "/v1/entitlement", "", token);
    if (reply.status == 401) store_.Clear();
    if (reply.status == 403 && reply.tier == "Free") return {true, false, "Free. No eligible paid subscription."};
    if (reply.status != 200) return Failure(reply.status);
    if (reply.tier == "Free") return {true, false, "Free. No eligible paid subscription."};
    if (reply.tier != "Pro" || reply.validUntil.empty() || reply.validUntil.size() > 12 ||
        !std::all_of(reply.validUntil.begin(), reply.validUntil.end(), [](char c) { return c >= '0' && c <= '9'; }))
        return {false, false, "Invalid entitlement response. Pro access denied."};
    const auto expires = std::stoll(reply.validUntil);
    if (expires <= clock_()) return {true, false, "Free. Paid subscription period expired."};
    // The result is transient. Every future Pro operation must call Check(true) again.
    return {true, true, "Pro subscription verified online. Paid features are not yet included in this Beta."};
}
Result Client::Logout() {
    const auto token = store_.Load();
    if (!HexToken(token)) { store_.Clear(); return {true, false, "Signed out."}; }
    const auto reply = transport_("POST", "/v1/auth/logout", "", token);
    if (reply.status != 200 && reply.status != 401)
        return {false, false, "Server logout failed. Session retained so you can retry --auth logout; Pro access is not verified."};
    if (!store_.Clear()) return {false, false, "Server session revoked, but Windows credential removal failed."};
    return {true, false, "Signed out; server session revoked."};
}
}
