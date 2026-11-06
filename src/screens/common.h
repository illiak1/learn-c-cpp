// common.h - drawing helpers shared by several pages (headers, lesson blocks, code blocks).
#pragma once

#include <string>
#include <vector>

#include "app/app.h"
#include "content/types.h"

namespace app {

// Page title with the track badge on the right and an optional muted subtitle; advances the column.
void pageHeader(ui::Column& col, App& app, const std::string& title, const std::string& subtitle = "");

// Draws lesson content blocks one after another and advances the column.
void drawBlocks(ui::Column& col, const std::vector<content::Block>& blocks, bool cpp, const std::string& idPrefix);

// Code block in the track language with a copy button; advances the column.
void drawCode(ui::Column& col, const std::string& id, const std::string& code, bool cpp, const std::string& caption = "",
              bool warn = false, bool plain = false);

// Monospace output box (program output); advances the column.
void drawOutput(ui::Column& col, const std::string& text, const std::string& caption = "");

// Small uppercase label.
void drawLabel(const std::string& text, float x, float y, Color c);

}  // namespace app
