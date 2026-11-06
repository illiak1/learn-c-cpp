// paths.h - locating bundled resources and the per-user data directory.
#pragma once

#include <filesystem>

namespace platform {

// Directory that contains the "assets" and "content" folders (searched around the executable and cwd).
std::filesystem::path resourceRoot();

// Writable per-user directory for progress files; falls back to a folder next to the executable.
// The environment variable LEARN_C_CPP_DATA overrides the location.
std::filesystem::path userDataDir();

}  // namespace platform
