#pragma once
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

inline void check_runtime_dependencies() {
    const auto inspect = [](std::string name) {
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        for (const char* forbidden : {"sycl", "ur_loader", "ur_adapter", "mkl_sycl"})
            if (name.find(forbidden) != std::string::npos)
                throw std::runtime_error("unexpected SYCL/UR/oneMKL runtime dependency: " + name);
    };
#ifdef _WIN32
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot inspect loaded modules");
    try {
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (!Module32FirstW(snapshot, &entry)) throw std::runtime_error("cannot enumerate loaded modules");
        do {
            char name[4 * (MAX_MODULE_NAME32 + 1)]{};
            if (!WideCharToMultiByte(CP_UTF8, 0, entry.szModule, -1, name, sizeof(name), nullptr, nullptr))
                throw std::runtime_error("cannot read loaded module name");
            inspect(name);
        } while (Module32NextW(snapshot, &entry));
        if (GetLastError() != ERROR_NO_MORE_FILES) throw std::runtime_error("incomplete loaded-module snapshot");
    } catch (...) { CloseHandle(snapshot); throw; }
    CloseHandle(snapshot);
#else
    std::ifstream maps("/proc/self/maps");
    if (!maps) throw std::runtime_error("cannot inspect process dependencies");
    std::string line;
    while (std::getline(maps, line)) {
        const auto slash = line.find_last_of('/');
        if (slash != std::string::npos) inspect(line.substr(slash + 1));
    }
    if (!maps.eof()) throw std::runtime_error("incomplete process memory map");
#endif
}
