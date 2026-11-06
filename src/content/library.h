// library.h - loads a track's content directory and cross-checks it.
#pragma once

#include <filesystem>
#include <string>

#include "content/types.h"

namespace content {

// Reads content/<name>/{lessons,exercises,exams} (sorted by file name). Problems end up in .errors.
TrackContent loadTrack(const std::filesystem::path& contentRoot, const std::string& name);

// Cross-reference checks: duplicate ids, unknown lesson/task ids, blank counts. Appends to .errors.
void validate(TrackContent& track);

// Reads a whole text file ("" when missing).
std::string readFile(const std::filesystem::path& path);

}  // namespace content
