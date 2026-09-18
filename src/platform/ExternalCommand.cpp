#include "ExternalCommand.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace bag {
namespace {

#if defined(_WIN32)

std::wstring widenUtf8(const std::string& value) {
    if (value.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), needed);
    return wide;
}

std::wstring quoteWindowsArg(const std::wstring& argument) {
    // Escape per Windows CommandLineToArgvW rules for CreateProcess.
    if (argument.find_first_of(L" \t\"") == std::wstring::npos) return argument;
    std::wstring quoted = L"\"";
    int backslashes = 0;
    for (wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            quoted.append(static_cast<std::size_t>(backslashes * 2 + 1), L'\\');
            quoted.push_back(L'"');
            backslashes = 0;
            continue;
        }
        if (backslashes > 0) {
            quoted.append(static_cast<std::size_t>(backslashes), L'\\');
            backslashes = 0;
        }
        quoted.push_back(ch);
    }
    if (backslashes > 0) quoted.append(static_cast<std::size_t>(backslashes * 2), L'\\');
    quoted.push_back(L'"');
    return quoted;
}

ExternalCommandResult runWindows(const std::vector<std::string>& argv) {
    ExternalCommandResult result;
    if (argv.empty()) {
        result.error = "empty external command";
        return result;
    }

    std::wstring commandLine;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i > 0) commandLine.push_back(L' ');
        commandLine += quoteWindowsArg(widenUtf8(argv[i]));
    }

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE stdoutRead = nullptr;
    HANDLE stdoutWrite = nullptr;
    if (!CreatePipe(&stdoutRead, &stdoutWrite, &security, 0)) {
        result.error = "CreatePipe failed";
        return result;
    }
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = stdoutWrite;
    startup.hStdError = stdoutWrite;

    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = commandLine;
    const BOOL ok = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process);
    CloseHandle(stdoutWrite);

    if (!ok) {
        CloseHandle(stdoutRead);
        result.error = "CreateProcessW failed for '" + argv[0] + "'";
        return result;
    }

    std::string output;
    std::array<char, 4096> buffer{};
    DWORD bytes = 0;
    while (ReadFile(stdoutRead, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes, nullptr) && bytes > 0) {
        output.append(buffer.data(), bytes);
    }
    CloseHandle(stdoutRead);

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    result.exitCode = static_cast<int>(exitCode);
    result.output = std::move(output);
    return result;
}

#else

ExternalCommandResult runPosix(const std::vector<std::string>& argv) {
    ExternalCommandResult result;
    if (argv.empty()) {
        result.error = "empty external command";
        return result;
    }

    int pipeFd[2] = {-1, -1};
    if (pipe(pipeFd) != 0) {
        result.error = "pipe failed";
        return result;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(pipeFd[0]);
        close(pipeFd[1]);
        result.error = "fork failed";
        return result;
    }

    if (pid == 0) {
        close(pipeFd[0]);
        dup2(pipeFd[1], STDOUT_FILENO);
        dup2(pipeFd[1], STDERR_FILENO);
        close(pipeFd[1]);

        std::vector<char*> raw;
        raw.reserve(argv.size() + 1);
        for (const auto& arg : argv) {
            raw.push_back(const_cast<char*>(arg.c_str()));
        }
        raw.push_back(nullptr);
        execvp(raw[0], raw.data());
        _exit(127);
    }

    close(pipeFd[1]);
    std::string output;
    std::array<char, 4096> buffer{};
    while (true) {
        const ssize_t n = read(pipeFd[0], buffer.data(), buffer.size());
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        output.append(buffer.data(), static_cast<std::size_t>(n));
    }
    close(pipeFd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        result.error = "waitpid failed";
        result.output = std::move(output);
        return result;
    }

    if (WIFEXITED(status)) result.exitCode = WEXITSTATUS(status);
    else result.exitCode = 1;
    result.output = std::move(output);
    if (result.exitCode == 127) result.error = "executable not found: " + argv[0];
    return result;
}

#endif

} // namespace

ExternalCommandResult runExternalCommand(const std::vector<std::string>& argv) {
#if defined(_WIN32)
    return runWindows(argv);
#else
    return runPosix(argv);
#endif
}

} // namespace bag
