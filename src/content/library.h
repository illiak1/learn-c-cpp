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

// Section title without a leftover leading number from the source notes ("1.17 Pass by value" -> "Pass by value").
std::string cleanSectionTitle(const std::string& title);

// Number shown for a section: <position of the lesson in the track>.<position of the section>, both 1-based.
// Derived from the order only, so content ids and titles can never produce odd numbers.
std::string sectionNumber(const TrackContent& track, const Lesson& lesson, size_t sectionIndex);

}  // namespace content
