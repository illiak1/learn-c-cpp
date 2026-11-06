// screens.h - factory functions for the page implementations.
#pragma once

#include <memory>

#include "app/app.h"

namespace app {

std::unique_ptr<Screen> makeHomeScreen();
std::unique_ptr<Screen> makeLessonsScreen();
std::unique_ptr<Screen> makeLessonScreen();
std::unique_ptr<Screen> makeExamSetupScreen();
std::unique_ptr<Screen> makeExamRunScreen();
std::unique_ptr<Screen> makePracticeScreen();
std::unique_ptr<Screen> makeExerciseScreen();
// Simple "coming soon" page with a title and a description.
std::unique_ptr<Screen> makePlaceholderScreen(const char* title, const char* description);

}  // namespace app
