// check_panel.h - the result panel of "Check from clipboard" on the exam run page (verdict, compiler messages,
// program output, per-function yes/no) and the progress card shown while the file is compiling or running.
#pragma once

#include <string>
#include <vector>

#include "content/types.h"
#include "logic/exam_check.h"
#include "logic/runner.h"
#include "ui/ui.h"

namespace app {

// What one check produced.
struct CheckPanel {
    logic::RunResult run;
    logic::ExamCheck check;
    int originTask = 0;        // task the check was started from
    std::string missing;       // function name not found in the clipboard: nothing was compiled or marked
    std::string missingNote;   // what the clipboard did contain
};

// Is this panel meant for the given task? Only the tasks of the checked file (or the one it was started from)
// show it, so switching tasks never shows another task's result.
bool panelBelongsTo(const CheckPanel& panel, size_t task);

// Draws the result blocks one under the other and advances the column.
void drawCheckPanel(ui::Column& col, const CheckPanel& panel, const std::vector<const content::Task*>& tasks);

// "Compiling..." / "Running..." card with a Cancel button (phase 1 = compiling, 2 = running); true when Cancel is clicked.
bool drawCheckProgress(ui::Column& col, int phase);

// How to install a compiler on this platform (shown when none is found).
std::string compilerInstallHint(bool cpp);

}  // namespace app
