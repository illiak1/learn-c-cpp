// paths.cpp - resource and user-data directory discovery using std::filesystem.
#include "platform/paths.h"

#include <cstdlib>
#include <fstream>
#include <system_error>
#include <vector>

#include "raylib.h"

namespace fs = std::filesystem;

namespace platform {

namespace {

// True when the directory holds both assets/fonts and content.
bool looksLikeRoot(const fs::path& dir) {
    std::error_code ec;
    return fs::is_directory(dir / "assets" / "fonts", ec) && fs::is_directory(dir / "content", ec);
}

// Directory of the running executable.
fs::path exeDir() { return fs::path(GetApplicationDirectory()); }

// Creates the directory and checks that a file can be written there.
bool writableDir(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec || !fs::is_directory(dir, ec)) return false;
    const fs::path probe = dir / ".write_test";
    {
        std::ofstream out(probe);
        if (!out) return false;
        out << "x";
    }
    fs::remove(probe, ec);
    return true;
}

// Reads an environment variable ("" when unset).
std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

}  // namespace

// Searches cwd, the executable folder and a few parents for the resource root.
fs::path resourceRoot() {
    std::vector<fs::path> candidates{fs::current_path(), exeDir(), exeDir() / "..", exeDir() / "../..",
                                     fs::current_path() / ".."};
    for (const fs::path& c : candidates) {
        if (looksLikeRoot(c)) return fs::weakly_canonical(c);
    }
    return fs::current_path();
}

// Chooses the per-user data directory with a portable fallback.
fs::path userDataDir() {
    const std::string forced = env("LEARN_C_CPP_DATA");
    if (!forced.empty() && writableDir(forced)) return forced;

    fs::path base;
#if defined(_WIN32)
    if (!env("APPDATA").empty()) base = fs::path(env("APPDATA")) / "learn-c-cpp";
#elif defined(__APPLE__)
    if (!env("HOME").empty()) base = fs::path(env("HOME")) / "Library" / "Application Support" / "learn-c-cpp";
#else
    if (!env("XDG_DATA_HOME").empty()) base = fs::path(env("XDG_DATA_HOME")) / "learn-c-cpp";
    else if (!env("HOME").empty()) base = fs::path(env("HOME")) / ".local" / "share" / "learn-c-cpp";
#endif
    if (!base.empty() && writableDir(base)) return base;

    fs::path fallback = exeDir() / "userdata";
    writableDir(fallback);
    return fallback;
}

}  // namespace platform
