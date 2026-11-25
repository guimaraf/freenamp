#pragma once

#include <string>

namespace freenamp {

// Quote one argument for the Windows C runtime command-line parser.
inline std::wstring quote_windows_argument(const std::wstring& argument) {
    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        quoted.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        quoted += character;
        backslashes = 0;
    }
    quoted.append(backslashes * 2, L'\\');
    return quoted + L'"';
}

} // namespace freenamp
