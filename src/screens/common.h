// common.h - drawing helpers shared by several pages (headers, lesson blocks, code blocks).
#pragma once

#include <string>
#include <vector>

#include "app/app.h"
#include "content/types.h"
#include "logic/progress.h"

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

// Colour of an exam result: green from 70 percent, orange from 40, red below (or when the file did not run).
Color examColor(const ui::Palette& p, const logic::ExamRecord& e);
// Whole-number percent of an exam result (0 when the file did not run).
int examPercent(const logic::ExamRecord& e);
// "2026-11-10 14:35" (the time is missing in old records).
std::string examWhen(const logic::ExamRecord& e);

// Small uppercase label.
void drawLabel(const std::string& text, float x, float y, Color c);

}  // namespace app
