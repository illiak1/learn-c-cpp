// plural.h - singular/plural wording for counters ("1 day", "2 days"), shared by every screen and logic text.
#pragma once

#include <string>

namespace logic {

// The noun alone, in the right number: unit(1, "day") = "day", unit(2, "day") = "days".
inline std::string unit(long n, const std::string& word) { return n == 1 ? word : word + "s"; }

// Count and noun together: plural(1, "day") = "1 day", plural(0, "day") = "0 days".
inline std::string plural(long n, const std::string& word) { return std::to_string(n) + " " + unit(n, word); }

}  // namespace logic
