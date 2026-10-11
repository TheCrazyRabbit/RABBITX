#include "entitlement_client.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <wincred.h>
#include <ctime>
#include <iostream>
#include <vector>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")

namespace RabbitXAuth {
namespace {
constexpr wchar_t CredentialName[] = L"CrazyRabbit/RABBITX/LiveSession/v1";
struct Handle {
    HINTERNET value;
    explicit Handle(HINTERNET v) : value(v) {}
    ~Handle() { if (value) WinHttpCloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
class WindowsStore final : public TokenStore {
    std::string Load() override {
        PCREDENTIALW credential = nullptr;
        if (!CredReadW(CredentialName, CRED_TYPE_GENERIC, 0, &credential)) return {};
        std::string token;
        if (credential->CredentialBlobSize == 64)
            token.assign(reinterpret_cast<char*>(credential->CredentialBlob), 64);
        CredFree(credential);
        return token;
    }
    bool Save(const std::string& token) override {
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = const_cast<LPWSTR>(CredentialName);
        credential.CredentialBlobSize = static_cast<DWORD>(token.size());
        credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(token.data()));
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE; // Current Windows user's credential vault.
        credential.UserName = const_cast<LPWSTR>(L"RABBITX session");
        return CredWriteW(&credential, 0) != FALSE;
    }
    bool Clear() override { return CredDeleteW(CredentialName, CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND; }
};
std::string Header(HINTERNET request, const wchar_t* name) {
    wchar_t buffer[256]{}; DWORD bytes = sizeof(buffer);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, name, buffer, &bytes, WINHTTP_NO_HEADER_INDEX)) return {};
    std::string value;
    for (const wchar_t* c = buffer; *c; ++c) { if (*c > 127) return {}; value += static_cast<char>(*c); }
    return value;
}
Reply Request(const std::string& method, const std::string& path, const std::string& body, const std::string& token) {
    // Dedicated identity transport: audit target scope/budget controls remain unchanged.
    Handle session(WinHttpOpen(L"RABBITX-Entitlement/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value || !WinHttpSetTimeouts(session.value, 10000, 10000, 10000, 10000)) return {};
    Handle connection(WinHttpConnect(session.value, L"api.crazyrabbit.dev", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection.value) return {};
    const std::wstring widePath(path.begin(), path.end()), wideMethod(method.begin(), method.end());
    Handle request(WinHttpOpenRequest(connection.value, wideMethod.c_str(), widePath.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.value) return {};
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect))) return {};
    std::wstring headers = L"Content-Type: application/json\r\nCache-Control: no-store\r\n";
    if (!token.empty()) headers += L"Authorization: Bearer " + std::wstring(token.begin(), token.end()) + L"\r\n";
    if (!WinHttpSendRequest(request.value, headers.c_str(), static_cast<DWORD>(headers.size()),
        body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0) || !WinHttpReceiveResponse(request.value, nullptr)) return {};
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) return {};
    // No TLS validation bypass. No redirects, cookies, caller-selected host or printed credentials.
    return {static_cast<int>(status), Header(request.value, L"X-RABBITX-Challenge"), Header(request.value, L"X-RABBITX-Session"),
        Header(request.value, L"X-RABBITX-Tier"), Header(request.value, L"X-RABBITX-Valid-Until")};
}
std::string ReadCode() {
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool console = GetConsoleMode(input, &mode) != FALSE;
    if (console && !SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT)) return {};
    std::string code; std::getline(std::cin, code);
    if (console) { SetConsoleMode(input, mode); std::cout << '\n'; }
    return code;
}
}
int RunCommand(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) != "--auth") return -1;
    if (argc != 3) { std::cerr << "Usage: RABBITX --auth login|status|check|logout\n"; return 1; }
    WindowsStore store;
    Client client(Request, store, [] { return static_cast<int64_t>(std::time(nullptr)); });
    const std::string command = argv[2];
    Result result;
    if (command == "login") {
        // Avoid overwriting a live session: revoke it before a new sign-in.
        result = client.Logout();
        if (!result.ok) { std::cerr << result.message << '\n'; return 1; }
        std::cout << "Paddle billing email: ";
        std::string email, challenge; std::getline(std::cin, email);
        result = client.Start(email, challenge);
        std::cout << result.message << '\n'; if (!result.ok) return 1;
        std::cout << "Email code: ";
        auto code = ReadCode(); result = client.Finish(challenge, code);
        if (!code.empty()) SecureZeroMemory(code.data(), code.size());
        std::cout << result.message << '\n'; if (!result.ok) return 1;
        result = client.Check();
    } else if (command == "status") result = client.Check();
    else if (command == "check") result = client.Check(true);
    else if (command == "logout") result = client.Logout();
    else { std::cerr << "Usage: RABBITX --auth login|status|check|logout\n"; return 1; }
    std::cout << result.message << '\n';
    return result.ok && (command != "check" || result.pro) ? 0 : 1;
}
}
