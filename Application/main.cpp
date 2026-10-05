#include "cli.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>

namespace
{
    std::string WideToUtf8(
        const wchar_t* value)
    {
        if (!value)
            return {};

        const int required = WideCharToMultiByte(
            CP_UTF8,
            0,
            value,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);

        if (required <= 0)
            return {};

        std::string result(
            static_cast<size_t>(required),
            '\0');

        if (WideCharToMultiByte(
            CP_UTF8,
            0,
            value,
            -1,
            result.data(),
            required,
            nullptr,
            nullptr) <= 0)
        {
            return {};
        }

        if (!result.empty() && result.back() == '\0')
            result.pop_back();

        return result;
    }
}

int wmain(
    int argc,
    wchar_t* argv[])
{
    std::vector<std::string> utf8Arguments;
    utf8Arguments.reserve(static_cast<size_t>(argc));

    for (int i = 0; i < argc; ++i)
    {
        utf8Arguments.push_back(
            WideToUtf8(argv[i]));
    }

    std::vector<char*> argumentPointers;
    argumentPointers.reserve(utf8Arguments.size());

    for (auto& argument : utf8Arguments)
    {
        argumentPointers.push_back(
            argument.data());
    }

    return CLI::Run(
        argc,
        argumentPointers.data());
}
