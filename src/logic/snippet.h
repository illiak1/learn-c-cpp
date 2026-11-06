// snippet.h - turns a code fragment into a complete, compilable program for "Copy to IDE".
#pragma once

#include <string>

namespace logic {

// Adds the usual #includes and a main() when the snippet has none.
std::string makeCompilable(const std::string& code, bool cpp);

}  // namespace logic
