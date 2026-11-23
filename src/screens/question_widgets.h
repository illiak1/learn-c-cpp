// question_widgets.h - drawing of the question parts shared by the exercise page and the Competition round.
#pragma once

#include "app/app.h"
#include "content/types.h"

namespace app {

// Option cards of a multiple-choice question; returns the index clicked this frame (-1 = none). Once `checked`, the
// right option is green, a wrong selection red, and the cards no longer react. Advances the column.
int drawChoiceCards(ui::Column& col, const content::Exercise& ex, int selected, bool checked);

// Code box of a "predict the output" question; advances the column.
void drawPredictCode(ui::Column& col, const content::Exercise& ex, bool cpp);

// Code box with clickable lines for "find the bug"; returns the 1-based line clicked this frame (0 = none).
// Once `checked`, the faulty line is highlighted (green bars when `lineOk`, else red) and clicks are ignored. Advances the column.
int drawBugCode(ui::Column& col, const content::Exercise& ex, bool cpp, int selectedLine, bool checked, bool lineOk = false);

}  // namespace app
