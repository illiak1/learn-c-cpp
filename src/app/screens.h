// screens.h - factory functions for the page implementations.
#pragma once

#include <memory>

#include "app/app.h"

namespace app {

std::unique_ptr<Screen> makeHomeScreen();
std::unique_ptr<Screen> makeLessonsScreen();
std::unique_ptr<Screen> makeLessonScreen();
// Simple "coming soon" page with a title and a description.
std::unique_ptr<Screen> makePlaceholderScreen(const char* title, const char* description);

}  // namespace app
