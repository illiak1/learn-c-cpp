// check_panel.cpp - result panel of "Check from clipboard": verdict, compiler messages, program output, functions.
#include "screens/check_panel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

#include "ui/theme.h"

namespace app {

using namespace ui;

namespace {

constexpr float kPad = 22.0f;
constexpr size_t kMaxMessages = 8;   // compiler messages listed before "+ N more"
constexpr size_t kMaxOutputLines = 14;

// "1 error", "3 warnings".
std::string plural(int n, const char* word) { return std::to_string(n) + " " + word + (n == 1 ? "" : "s"); }

// "0.31 s".
std::string seconds(double s) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f s", s);
    return buf;
}

// Badge colours of a run verdict.
void verdictColors(const Palette& p, const CheckPanel& panel, Color& fg, Color& bg) {
    using logic::Verdict;
    switch (panel.run.verdict) {
        case Verdict::CompileError:
        case Verdict::Timeout:
        case Verdict::Crash: fg = p.danger; bg = p.dangerSoft; break;
        case Verdict::Finished: {
            const bool allYes = std::all_of(panel.check.tasks.begin(), panel.check.tasks.end(),
                                            [](const logic::TaskVerdict& t) { return t.status == logic::TaskCheck::Passed; });
            fg = allYes ? p.success : p.accent;
            bg = allYes ? p.successSoft : p.accentSoft;
            break;
        }
        default: fg = p.warning; bg = p.warningSoft; break;
    }
}

// One-line summary under the verdict: compiler and timings.
std::string summaryLine(const logic::RunResult& r) {
    std::string s;
    auto add = [&](const std::string& part) { s += (s.empty() ? "" : "  \xC2\xB7  ") + part; };
    if (!r.compiler.empty()) add(r.compiler);
    if (r.compileSeconds > 0.0) add("compiled in " + seconds(r.compileSeconds));
    if (r.runSeconds > 0.0) add((r.verdict == logic::Verdict::Timeout ? "stopped after " : "ran in ") + seconds(r.runSeconds));
    if (r.verdict == logic::Verdict::Finished) add("exit code " + std::to_string(r.exitCode));
    return s;
}

// Callout with an icon and wrapped text (bold first line optional); advances the column.
void noteCallout(Column& col, Color accent, Color soft, Icon icon, const std::string& title, const std::string& detail) {
    const Palette& p = Theme::get().colors();
    const float textW = col.w - 80.0f;
    const float titleH = title.empty() ? 0.0f : textHeight(Face::Bold, size::kBodySm, title, textW);
    const float detailH = detail.empty() ? 0.0f : textHeight(Face::Regular, size::kSmall, detail, textW);
    const float h = std::max(48.0f, titleH + detailH + (titleH > 0 && detailH > 0 ? 4.0f : 0.0f) + 26.0f);
    Rect r = col.take(h);
    callout(r, accent, soft);
    drawIcon(icon, r.x + 30.0f, r.y + r.height / 2.0f, 18.0f, accent);
    float y = r.y + (r.height - (titleH + detailH + (titleH > 0 && detailH > 0 ? 4.0f : 0.0f))) / 2.0f;
    if (titleH > 0) y += drawParagraph(Face::Bold, size::kBodySm, title, r.x + 54.0f, y, textW, p.text) + 4.0f;
    if (detailH > 0) drawParagraph(Face::Regular, size::kSmall, detail, r.x + 54.0f, y, textW, p.textMuted);
}

// Errors first, then warnings (notes are not listed).
std::vector<const logic::Diagnostic*> listedMessages(const logic::RunResult& run) {
    std::vector<const logic::Diagnostic*> out;
    for (const char* sev : {"error", "warning"})
        for (const logic::Diagnostic& d : run.diagnostics)
            if (d.severity == sev) out.push_back(&d);
    return out;
}

// Card with the compiler messages as a list.
void drawMessages(Column& col, const logic::RunResult& run) {
    const Palette& p = Theme::get().colors();
    const auto msgs = listedMessages(run);
    if (msgs.empty()) return;
    const size_t shown = std::min(msgs.size(), kMaxMessages);
    const float posW = 108.0f;
    const float msgW = col.w - 2.0f * kPad - posW - 18.0f;
    std::vector<float> heights;
    float h = kPad + 36.0f;
    for (size_t i = 0; i < shown; ++i) {
        heights.push_back(std::max(30.0f, textHeight(Face::Regular, size::kSmall, msgs[i]->message, msgW) + 12.0f));
        h += heights.back();
    }
    if (msgs.size() > shown) h += 30.0f;
    h += 10.0f;
    Rect r = col.take(h);
    card(r);
    const int errors = run.count("error"), warnings = run.count("warning");
    drawText(Face::Bold, size::kH3, "Compiler messages", r.x + kPad, r.y + 16.0f, p.text);
    const std::string counts = (errors ? plural(errors, "error") : std::string()) + (errors && warnings ? ", " : "") + (warnings ? plural(warnings, "warning") : std::string());
    const float tw = Theme::get().textWidth(Face::Regular, size::kSmall, counts);
    drawText(Face::Regular, size::kSmall, counts, r.x + r.width - kPad - tw, r.y + 22.0f, p.textMuted);
    float y = r.y + kPad + 36.0f;
    for (size_t i = 0; i < shown; ++i) {
        const logic::Diagnostic& d = *msgs[i];
        const bool err = d.severity == "error";
        const Color c = err ? p.danger : p.warning;
        const float rowH = heights[i];
        DrawCircleV(Vector2{r.x + kPad + 5.0f, y + rowH / 2.0f}, 4.5f, c);
        std::string pos = d.line > 0 ? "Line " + std::to_string(d.line) + (d.column > 0 ? ":" + std::to_string(d.column) : std::string()) : "Linker";
        drawText(Face::Bold, size::kSmall, pos, r.x + kPad + 18.0f, y + rowH / 2.0f - Theme::get().textMidline(Face::Bold, size::kSmall), c);
        drawParagraph(Face::Regular, size::kSmall, d.message, r.x + kPad + 18.0f + posW, y + 6.0f, msgW, p.text);
        y += rowH;
    }
    if (msgs.size() > shown)
        drawText(Face::Regular, size::kSmall, "+ " + std::to_string(msgs.size() - shown) + " more (fix the first ones and check again)", r.x + kPad + 18.0f, y + 6.0f, p.textMuted);
}

// First lines of the program output, each cut to a sensible width.
std::string outputPreview(const std::string& output, size_t& totalLines) {
    std::istringstream in(output);
    std::string line, out;
    totalLines = 0;
    while (std::getline(in, line)) {
        if (totalLines < kMaxOutputLines) out += (totalLines ? "\n" : "") + (line.size() > 160 ? line.substr(0, 157) + "..." : line);
        ++totalLines;
    }
    return out;
}

// Program output in a code box (first lines only) with a note when it is cut.
void drawProgramOutput(Column& col, const logic::RunResult& run) {
    const Palette& p = Theme::get().colors();
    if (run.output.empty()) {
        if (run.verdict == logic::Verdict::Finished) {
            Rect r = col.take(40.0f);
            card(r);
            drawText(Face::Regular, size::kSmall, "The program printed nothing.", r.x + kPad, r.y + 10.0f, p.textMuted);
        }
        return;
    }
    size_t total = 0;
    const std::string shown = outputPreview(run.output, total);
    CodeBoxOpts o;
    o.plain = true;
    o.lineNumbers = false;
    o.title = "Program output";
    const float h = codeBoxHeight(shown, o);
    codeBox("check-output", Rect{col.x, col.y, col.w, h}, shown, o);
    col.space(h + 6.0f);
    std::string note;
    if (total > kMaxOutputLines) note = "Showing the first " + std::to_string(kMaxOutputLines) + " of " + std::to_string(total) + " lines. ";
    if (run.outputTruncated) note += "The program printed more than 64 KB; the rest was dropped.";
    if (!note.empty()) col.space(drawParagraph(Face::Regular, size::kSmall, note, col.x + 4.0f, col.y, col.w - 8.0f, p.textMuted) + 6.0f);
    col.space(6.0f);
}

// Badge text and colours of one function's result.
void functionBadge(const Palette& p, logic::TaskCheck c, std::string& text, Color& fg, Color& bg) {
    switch (c) {
        case logic::TaskCheck::Passed: text = "yes"; fg = p.success; bg = p.successSoft; break;
        case logic::TaskCheck::Failed: text = "no"; fg = p.danger; bg = p.dangerSoft; break;
        case logic::TaskCheck::NoOutput: text = "no output"; fg = p.danger; bg = p.dangerSoft; break;
        case logic::TaskCheck::NotChecked: text = "check by eye"; fg = p.textMuted; bg = p.surfaceAlt; break;
    }
}

// Card with a yes/no row per function.
void drawFunctions(Column& col, const CheckPanel& panel, const std::vector<const content::Task*>& tasks) {
    const Palette& p = Theme::get().colors();
    const size_t n = std::min(tasks.size(), panel.check.tasks.size());
    if (n == 0) return;
    const bool anyUnchecked = std::any_of(panel.check.tasks.begin(), panel.check.tasks.end(),
                                          [](const logic::TaskVerdict& t) { return t.status == logic::TaskCheck::NotChecked; });
    const std::string foot = panel.check.wholeExamZero
                                 ? "Every function is marked Not solved because the whole exam gives 0. Fix the problem and check again."
                                 : anyUnchecked ? "The tests of a function marked \"check by eye\" print values instead of yes/no: compare them with the paper and mark it yourself."
                                                : "The functions were marked automatically. You can still change the marks below.";
    const float footH = textHeight(Face::Regular, size::kSmall, foot, col.w - 2.0f * kPad);
    Rect r = col.take(kPad + 36.0f + 36.0f * static_cast<float>(n) + footH + 20.0f);
    card(r);
    drawText(Face::Bold, size::kH3, "Functions", r.x + kPad, r.y + 16.0f, p.text);
    int solved = 0;
    for (size_t i = 0; i < n; ++i) solved += panel.check.marks[i] == 1;
    if (!panel.check.wholeExamZero) {
        const std::string s = std::to_string(solved) + " of " + std::to_string(n) + " yes";
        drawText(Face::Regular, size::kSmall, s, r.x + r.width - kPad - Theme::get().textWidth(Face::Regular, size::kSmall, s), r.y + 22.0f, p.textMuted);
    }
    float y = r.y + kPad + 36.0f;
    for (size_t i = 0; i < n; ++i) {
        const logic::TaskVerdict& v = panel.check.tasks[i];
        std::string text;
        Color fg, bg;
        functionBadge(p, v.status, text, fg, bg);
        const float bw = badgeWidth(text);
        badge(text, r.x + r.width - kPad - bw, y + 5.0f, fg, bg);
        float rightEdge = r.x + r.width - kPad - bw - 12.0f;
        if (v.yes + v.no > 0 && !panel.check.wholeExamZero) {
            const std::string counts = std::to_string(v.yes) + " yes, " + std::to_string(v.no) + " no";
            const float cw = Theme::get().textWidth(Face::Regular, size::kSmall, counts);
            drawText(Face::Regular, size::kSmall, counts, rightEdge - cw, y + 18.0f - Theme::get().textMidline(Face::Regular, size::kSmall), p.textMuted);
            rightEdge -= cw + 12.0f;
        }
        const std::string label = "Task " + std::to_string(i + 1) + "  \xC2\xB7  " + tasks[i]->title;
        drawText(Face::Regular, size::kBodySm, ellipsize(Face::Regular, size::kBodySm, label, rightEdge - (r.x + kPad)), r.x + kPad,
                 y + 18.0f - Theme::get().textMidline(Face::Regular, size::kBodySm), p.text);
        y += 36.0f;
    }
    drawParagraph(Face::Regular, size::kSmall, foot, r.x + kPad, y + 6.0f, col.w - 2.0f * kPad, p.textMuted);
}

}  // namespace

// Verdict card, then the explanation, compiler messages, output and per-function list.
void drawCheckPanel(Column& col, const CheckPanel& panel, const std::vector<const content::Task*>& tasks) {
    const Palette& p = Theme::get().colors();
    const logic::RunResult& run = panel.run;

    Rect head = col.take(64.0f);
    card(head);
    Color fg, bg;
    verdictColors(p, panel, fg, bg);
    const std::string name = logic::verdictName(run.verdict);
    const float bw = badgeWidth(name);
    badge(name, head.x + kPad, head.y + 18.0f, fg, bg);
    const std::string summary = summaryLine(run);
    drawText(Face::Regular, size::kSmall, ellipsize(Face::Regular, size::kSmall, summary, head.width - 2.0f * kPad - bw - 14.0f),
             head.x + kPad + bw + 14.0f, head.y + 32.0f - Theme::get().textMidline(Face::Regular, size::kSmall), p.textMuted);

    using logic::Verdict;
    if (panel.check.wholeExamZero) {
        std::string detail;
        if (run.verdict == Verdict::CompileError) detail = "The file does not compile. Fix the errors below and check again.";
        else if (run.verdict == Verdict::Timeout) detail = run.detail;
        else detail = "The program crashed: " + run.signalName + ".";
        noteCallout(col, p.danger, p.dangerSoft, Icon::Warning, "In the real exam this gives 0 for the whole exam", detail);
    } else if (run.verdict == Verdict::CompilerMissing || run.verdict == Verdict::InternalError) {
        noteCallout(col, p.warning, p.warningSoft, Icon::Warning, run.detail, "");
    } else if (run.verdict == Verdict::Cancelled) {
        noteCallout(col, p.warning, p.warningSoft, Icon::Warning, "The check was cancelled", "Nothing was marked.");
    }

    drawMessages(col, run);
    if (run.verdict == Verdict::Finished || run.verdict == Verdict::Timeout || run.verdict == Verdict::Crash) drawProgramOutput(col, run);
    if (run.verdict == Verdict::Finished || panel.check.wholeExamZero) drawFunctions(col, panel, tasks);
}

// Card shown while the worker compiles or runs the file.
bool drawCheckProgress(Column& col, int phase) {
    const Palette& p = Theme::get().colors();
    Rect r = col.take(64.0f);
    card(r);
    const int dots = static_cast<int>(GetTime() * 3.0) % 4;
    const std::string text = std::string(phase == 2 ? "Running the program" : "Compiling") + std::string(static_cast<size_t>(dots), '.');
    drawIcon(Icon::Timer, r.x + kPad + 10.0f, r.y + r.height / 2.0f, 19.0f, p.accent);
    drawText(Face::Bold, size::kBody, text, r.x + kPad + 36.0f, r.y + r.height / 2.0f - Theme::get().textMidline(Face::Bold, size::kBody), p.text);
    return button("Cancel", Rect{r.x + r.width - kPad - 120.0f, r.y + 12.0f, 120.0f, 40.0f}, ButtonStyle::Danger, Icon::Cross);
}

// Platform-specific install advice.
std::string compilerInstallHint(bool cpp) {
    const std::string what = cpp ? "a C++ compiler (g++ or clang++)" : "a C compiler (gcc or clang)";
#if defined(_WIN32)
    return "No compiler found. Install MSYS2 or MinGW-w64 (it provides " + what + "), add its bin folder to PATH, then press Detect again.";
#elif defined(__APPLE__)
    return "No compiler found. Run xcode-select --install in a terminal to get " + what + ", then press Detect again.";
#else
    return "No compiler found. Install " + what + ", e.g. sudo apt install build-essential, then press Detect again.";
#endif
}

}  // namespace app
