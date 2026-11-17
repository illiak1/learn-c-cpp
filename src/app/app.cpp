// app.cpp - window setup, main loop, sidebar navigation and screen switching.
#include "app/app.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <cstdio>
#include <filesystem>
#include <cstdlib>
#include <random>

#include "app/screens.h"
#include "content/library.h"
#include "platform/paths.h"
#include "raylib.h"

namespace app {

using namespace ui;

namespace {
constexpr float kBottomPadding = 96.0f;  // empty space kept below the last element of every page
constexpr int kMinW = 960, kMinH = 600;  // smallest window, in logical pixels

// Applies the minimum window size. The window system counts physical pixels on Windows and X11,
// so the limit grows with the display scale there; macOS already works in logical points.
void applyMinSize(float scale) {
#if defined(__APPLE__)
    (void)scale;
    SetWindowMinSize(kMinW, kMinH);
#else
    SetWindowMinSize(static_cast<int>(static_cast<float>(kMinW) * scale), static_cast<int>(static_cast<float>(kMinH) * scale));
#endif
}
}  // namespace

// ---- Small helpers --------------------------------------------------------------------------

Section sectionOf(Page page) {
    switch (page) {
        case Page::Home: return Section::Home;
        case Page::Lessons:
        case Page::Lesson: return Section::Lessons;
        case Page::Practice:
        case Page::Exercise: return Section::Practice;
        case Page::Exam:
        case Page::ExamRun:
        case Page::ExamReview: return Section::Exam;
        case Page::Mistakes: return Section::Mistakes;
        case Page::Stats: return Section::Stats;
    }
    return Section::Home;
}

const char* trackName(Track t) { return t == Track::C ? "C" : "C++"; }


// Parses --dark, --cpp, --page=NAME and --screenshot=FILE.
Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--dark") { o.dark = true; o.hasTheme = true; }
        else if (a == "--light") { o.dark = false; o.hasTheme = true; }
        else if (a == "--cpp") { o.track = Track::Cpp; o.hasTrack = true; }
        else if (a == "--c") { o.track = Track::C; o.hasTrack = true; }
        else if (a.rfind("--page=", 0) == 0) {
            const std::string n = a.substr(7);
            if (n == "lessons") o.page = Page::Lessons;
            else if (n == "practice") o.page = Page::Practice;
            else if (n == "exam") o.page = Page::Exam;
            else if (n == "mistakes") o.page = Page::Mistakes;
            else if (n == "examrun") o.page = Page::ExamRun;
            else if (n == "review") o.page = Page::ExamReview;
            else if (n == "lesson") o.page = Page::Lesson;
            else if (n == "exercise") o.page = Page::Exercise;
            else if (n == "stats") o.page = Page::Stats;
        } else if (a.rfind("--lesson=", 0) == 0) {
            o.lesson = a.substr(9);
        } else if (a.rfind("--size=", 0) == 0) {
            std::sscanf(a.c_str() + 7, "%dx%d", &o.width, &o.height);
        } else if (a.rfind("--question=", 0) == 0) {
            o.question = std::atoi(a.c_str() + 11);
        } else if (a.rfind("--section=", 0) == 0) {
            o.section = std::atoi(a.c_str() + 10);
        } else if (a.rfind("--screenshot=", 0) == 0) {
            o.screenshot = a.substr(13);
        }
    }
    return o;
}

// ---- Screen base ----------------------------------------------------------------------------

// Opens the scroll view and returns a centred column (max width kContentMaxW).
Column Screen::begin(Rect area) {
    viewport_ = area;
    Rect content = scrollBegin(scroll_, area);
    const float w = std::min(metrics::kContentMaxW, content.width - 64.0f);
    Column col;
    col.w = w;
    col.x = content.x + (content.width - w) / 2.0f;
    col.y = content.y + 32.0f;
    return col;
}

// Ends the scroll view with generous bottom padding so the last button is never flush with the window edge.
void Screen::end(const Column& col) { scrollEnd(scroll_, viewport_, col.y + kBottomPadding); }

// ---- App ------------------------------------------------------------------------------------

App::App(Options options) : options_(std::move(options)), track_(options_.track), page_(options_.page) {}

App::~App() = default;

// Registers every page implementation.
void App::registerScreens() {
    screens_[Page::Home] = makeHomeScreen();
    screens_[Page::Lessons] = makeLessonsScreen();
    screens_[Page::Lesson] = makeLessonScreen();
    screens_[Page::Practice] = makePracticeScreen();
    screens_[Page::Exercise] = makeExerciseScreen();
    screens_[Page::Exam] = makeExamSetupScreen();
    screens_[Page::ExamRun] = makeExamRunScreen();
    screens_[Page::ExamReview] = makeExamReviewScreen();
    screens_[Page::Mistakes] = makeMistakesScreen();
    screens_[Page::Stats] = makeStatsScreen();
}

// Switches the track, remembers it in the settings and returns to the home page.
void App::setTrack(Track t) {
    if (t == track_) return;
    track_ = t;
    Theme::get().setAccent(t == Track::C ? Accent::C : Accent::Cpp);
    progress_.track = trackKey();
    saveProgress();
    go(Page::Home);
}

// Opens the lesson reader on a lesson (and section index).
void App::openLesson(const std::string& lessonId, int section) {
    lessonId_ = lessonId;
    lessonSection_ = section;
    go(Page::Lesson);
}

// Starts a practice session on the exercise page.
void App::startSession(const std::string& title, std::vector<std::string> ids, bool retry) {
    session_.title = title;
    session_.retry = retry;
    session_.ids = std::move(ids);
    session_.pos = 0;
    session_.results.assign(session_.ids.size(), -1);
    go(Page::Exercise);
}

// Writes the progress file when something changed.
void App::saveProgress() {
    if (progress_.dirty()) progress_.save();
}

// Number of wrong questions plus failed exam functions waiting for a retry.
int App::mistakeCount() {
    return static_cast<int>(progress_.wrongExercises(trackKey()).size() + progress_.failedTasks(trackKey()).size());
}

// Writes the finished exam to the progress file (one record per exam) and keeps it in sync with the review marks.
void App::commitExam() {
    if (!exam_.finished) return;
    const auto tasks = logic::examTasks(content(), exam_);
    const logic::Score sc = logic::scoreExam(content(), exam_);
    logic::ExamRecord r;
    r.date = exam_.finishDate;
    r.time = exam_.finishTime;
    r.track = trackKey();
    r.title = exam_.title;
    r.earned = sc.earned;
    r.total = sc.total;
    r.seconds = static_cast<int>(exam_.finishedAt - exam_.startedAt);
    r.fileRan = exam_.fileRan;
    std::string sig = std::to_string(sc.earned) + (exam_.fileRan ? "y" : "n");
    for (size_t i = 0; i < tasks.size(); ++i) {
        const signed char mark = i < exam_.marks.size() ? exam_.marks[i] : -1;
        // Unmarked tasks score 0; so does everything when the file did not compile, loop forever or crash.
        r.tasks.push_back({tasks[i]->id, exam_.fileRan && mark == 1 ? tasks[i]->points : 0, tasks[i]->points});
        sig += std::to_string(mark);
    }
    if (exam_.recordIndex >= 0 && sig == exam_.recordSig) return;
    for (size_t i = 0; i < tasks.size() && i < exam_.marks.size(); ++i)
        if (exam_.marks[i] >= 0) progress_.recordTask(trackKey(), tasks[i]->id, exam_.marks[i] == 1, r.date);
    if (exam_.recordIndex < 0) exam_.recordIndex = progress_.recordExam(r);
    else progress_.updateExam(exam_.recordIndex, r);
    exam_.recordSig = sig;
    saveProgress();
}

// Begins an exam: stamps the start time and opens the run page.
void App::startExam(logic::Exam exam) {
    exam.startedAt = GetTime();
    exam.finished = false;
    exam.recordIndex = -1;
    exam.recordSig.clear();
    exam_ = std::move(exam);
    go(Page::ExamRun);
}

// Starts the exam at once, or asks before throwing away an exam that is still running.
void App::beginExam(logic::Exam exam) {
    if (!exam_.taskIds.empty() && !exam_.finished) {
        pendingExam_ = std::move(exam);
        openModal("discard-exam", ModalOpts{"Start a new exam?", "An exam is still running. Starting a new one discards it without a result.",
                                            "Start new exam", "Keep running", true});
        return;
    }
    startExam(std::move(exam));
}

// Asks for confirmation before ending the running exam; the clock keeps running meanwhile.
// The code is written and tested in the student's IDE, so the text only talks about the self-check marks.
void App::requestFinishExam() {
    const bool anySolved = std::count(exam_.marks.begin(), exam_.marks.end(), static_cast<signed char>(1)) > 0;
    const std::string message = anySolved ? "Finish the exam now? Tasks not marked as solved will not count."
                                          : "Finish the exam now? No tasks are marked as solved yet.";
    openModal("finish-exam", ModalOpts{"Finish exam", message + " You can still change the marks in the review.", "Finish exam", "Keep working", true});
}

// Asks for confirmation before erasing answers, streak and exam history.
void App::requestResetProgress() {
    openModal("reset-progress", ModalOpts{"Reset progress", "Erase all answers, the streak and the exam history? This cannot be undone.",
                                          "Reset progress", "Keep progress", true});
}

// Stops the clock and opens the review.
void App::finishExam() {
    if (!exam_.finished) {
        exam_.finished = true;
        exam_.finishedAt = GetTime();
        exam_.finishDate = logic::todayString();
        exam_.finishTime = logic::nowTimeString();
        commitExam();
    }
    go(Page::ExamReview);
}

// Switches page and notifies the new screen.
void App::go(Page page) {
    closeModal();
    Context::get().enterPage();
    page_ = page;
    screens_[page_]->onEnter(*this);
}

// Rectangle to the right of the sidebar.
Rect App::contentArea() const {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    return Rect{metrics::kSidebarW, 0.0f, w - metrics::kSidebarW, h};
}

// Left navigation: logo, track toggle, page list with a sliding active pill, theme switch.
void App::drawSidebar(Rect r) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    DrawRectangleRec(r, p.sidebar);
    DrawLineEx(Vector2{r.x + r.width - 0.5f, r.y}, Vector2{r.x + r.width - 0.5f, r.y + r.height}, 1.0f, p.border);

    // Logo: a gradient badge in the track colour that names the selected track
    Rect mark{r.x + 20.0f, r.y + 22.0f, 40.0f, 40.0f};
    fillRoundGradient(mark, radius::kMd + 2.0f, mix(p.accent, WHITE, 0.2f), p.accent);
    strokeRound(mark, radius::kMd + 2.0f, 1.0f, withAlpha(WHITE, 0.18f));
    drawCentered(Face::Bold, track_ == Track::C ? 21 : 17, trackName(track_), mark, p.textOnAccent);
    drawText(Face::Bold, 18, "Exam Trainer", r.x + 72.0f, r.y + 23.0f, p.text);
    drawText(Face::Regular, size::kLabel, "C and C++ practice", r.x + 72.0f, r.y + 45.0f, p.textMuted);

    // Track toggle
    drawText(Face::Bold, size::kLabel, "TRACK", r.x + 22.0f, r.y + 86.0f, p.textMuted);
    const int sel = tabs(Rect{r.x + 16.0f, r.y + 108.0f, r.width - 32.0f, 42.0f}, {"C", "C++"}, track_ == Track::C ? 0 : 1);
    if (sel != (track_ == Track::C ? 0 : 1)) setTrack(sel == 0 ? Track::C : Track::Cpp);

    // Navigation items
    struct Item { const char* label; Icon icon; Section section; Page page; };
    static const Item items[] = {
        {"Home", Icon::Home, Section::Home, Page::Home},
        {"Lessons", Icon::Book, Section::Lessons, Page::Lessons},
        {"Practice", Icon::Pencil, Section::Practice, Page::Practice},
        {"Exam mode", Icon::Timer, Section::Exam, Page::Exam},
        {"Mistakes", Icon::Cross, Section::Mistakes, Page::Mistakes},
        {"Statistics", Icon::Chart, Section::Stats, Page::Stats},
    };
    drawText(Face::Bold, size::kLabel, "LEARN", r.x + 22.0f, r.y + 176.0f, p.textMuted);
    const float top = r.y + 200.0f, pitch = 46.0f, rowH = 42.0f;
    int activeIdx = 0;
    for (int i = 0; i < 6; ++i)
        if (sectionOf(page_) == items[i].section) activeIdx = i;
    // The pill and its accent bar glide to the active row.
    const float pillY = cx.tween(cx.makeId("nav-pill"), top + pitch * static_cast<float>(activeIdx), motion::kBase);
    const Rect pill{r.x + 12.0f, pillY, r.width - 24.0f, rowH};
    fillRound(pill, radius::kMd, p.accentSoft);
    fillRound(Rect{pill.x, pill.y + 10.0f, 4.0f, rowH - 20.0f}, 2.0f, p.accent);
    float y = top;
    for (const Item& it : items) {
        Rect row{r.x + 12.0f, y, r.width - 24.0f, rowH};
        const uint32_t id = cx.makeId(it.label);
        const bool hov = cx.hovered(row);
        if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
        const float h = cx.anim(id, hov ? 1.0f : 0.0f);
        const float cov = std::clamp(1.0f - std::fabs(pill.y - row.y) / pitch, 0.0f, 1.0f);
        if (h > 0.0f) fillRound(row, radius::kMd, withAlpha(p.surfaceHover, h * (1.0f - cov) * 0.8f));
        const Color fg = mix(mix(p.textMuted, p.text, h), p.accent, cov);
        drawIcon(it.icon, row.x + 26.0f, row.y + row.height / 2.0f, 19.0f, fg);
        drawText(cov > 0.5f ? Face::Bold : Face::Regular, size::kButton, it.label, row.x + 50.0f, row.y + 10.0f, fg);
        if (it.section == Section::Mistakes) {
            const int n = mistakeCount();
            if (n > 0) badge(std::to_string(n), row.x + row.width - badgeWidth(std::to_string(n)) - 10.0f, row.y + 10.0f, WHITE, p.dangerFill);
        }
        if (hov && cx.pressed && page_ != it.page) go(it.page);
        y += pitch;
    }

    // Theme switch at the bottom
    const Rect foot{r.x + 12.0f, r.y + r.height - 66.0f, r.width - 24.0f, 50.0f};
    fillRound(foot, radius::kMd, p.surfaceAlt);
    const bool dark = th.mode() == ThemeMode::Dark;
    drawIcon(dark ? Icon::Moon : Icon::Sun, foot.x + 24.0f, foot.y + foot.height / 2.0f, 18.0f, p.textMuted, p.surfaceAlt);
    drawText(Face::Regular, size::kBodySm, dark ? "Dark theme" : "Light theme", foot.x + 46.0f, foot.y + 14.0f, p.text);
    bool v = dark;
    if (toggleSwitch("theme-switch", Rect{foot.x + foot.width - 56.0f, foot.y + 13.0f, 42.0f, 24.0f}, v)) {
        th.setMode(v ? ThemeMode::Dark : ThemeMode::Light);
        progress_.dark = v;
        progress_.dirtyFlag();
        saveProgress();
    }
}

// One frame: input, sidebar, current page.
void App::frame() {
    Context& cx = Context::get();
    // Follow the display scale (it changes when the window moves to another monitor).
    const float scale = GetWindowScaleDPI().x;
    if (scale != Theme::get().scale()) {
        Theme::get().setScale(scale);
        applyMinSize(Theme::get().scale());
    }
    cx.beginFrame();
    const Palette& p = Theme::get().colors();
    BeginDrawing();
    ClearBackground(p.bg);
    // Answers of dialogs opened in the previous frame.
    if (takeModalResult("finish-exam") == ModalResult::Confirm) finishExam();
    if (takeModalResult("discard-exam") == ModalResult::Confirm) startExam(std::move(pendingExam_));
    if (takeModalResult("reset-progress") == ModalResult::Confirm) {
        progress_.reset();
        saveProgress();
        cx.toast("Progress erased");
    }
    // Page transition: the content slides up a few pixels while a veil of the background colour fades out.
    const float pt = cx.pageProgress();
    const Rect area = contentArea();
    if (pt < 1.0f) {
        cx.pushClip(area);
        pushOffset(0.0f, (1.0f - pt) * 10.0f);
    }
    screens_[page_]->draw(*this, area);
    if (pt < 1.0f) {
        popOffset();
        DrawRectangleRec(area, withAlpha(p.bg, 1.0f - pt));
        cx.popClip();
    }
    drawSidebar(Rect{0.0f, 0.0f, metrics::kSidebarW, static_cast<float>(GetScreenHeight())});
    drawModal();
    cx.endFrame();
    EndDrawing();
}

// Opens the window, loads content and progress, and runs the frame loop until the window closes.
int App::run() {
    // High-DPI: the layout stays in logical pixels, Raylib scales the frame and the fonts are rasterised
    // at the physical size (see Theme::setScale), so text is not stretched on scaled displays.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(options_.width, options_.height, "Exam Trainer - C and C++");
    applyMinSize(GetWindowScaleDPI().x);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    Context::get().ignoreMouse = !options_.screenshot.empty();

    Theme& th = Theme::get();
    th.init((platform::resourceRoot() / "assets").string());
    th.setScale(GetWindowScaleDPI().x);
    th.preload();
    const auto progressFile = platform::userDataDir() / "progress.txt";
    progress_.load(progressFile);
    if (progress_.badLines() > 0) {
        // Keep a copy of a damaged file before the next save rewrites it.
        std::error_code ec;
        std::filesystem::copy_file(progressFile, progressFile.string() + ".bak", std::filesystem::copy_options::overwrite_existing, ec);
        std::fprintf(stderr, "progress: %d unreadable line(s) skipped, backup written to progress.txt.bak\n", progress_.badLines());
    }
    if (!options_.hasTheme) options_.dark = progress_.dark;
    if (!options_.hasTrack) options_.track = progress_.track == "cpp" ? Track::Cpp : Track::C;
    track_ = options_.track;
    th.setAccent(track_ == Track::C ? Accent::C : Accent::Cpp);
    th.setMode(options_.dark ? ThemeMode::Dark : ThemeMode::Light);
    // Load both tracks and report problems on the console.
    const auto root = platform::resourceRoot() / "content";
    tracks_[0] = content::loadTrack(root, "c");
    tracks_[1] = content::loadTrack(root, "cpp");
    for (const auto& t : tracks_)
        for (const std::string& e : t.errors) std::fprintf(stderr, "content[%s]: %s\n", t.name.c_str(), e.c_str());
    registerScreens();
    if (options_.page == Page::Lesson) {
        const auto& ls = content().lessons;
        const bool known = !options_.lesson.empty() && content().findLesson(options_.lesson);
        if (known) openLesson(options_.lesson, options_.section);
        else if (!ls.empty()) openLesson(ls.front().id, options_.section);
        else go(Page::Lessons);
    } else if (options_.page == Page::Exercise) {
        std::vector<std::string> ids;
        for (const auto& e : content().exercises) ids.push_back(e.id);
        startSession("Sample session", ids);
        session_.pos = static_cast<size_t>(std::max(0, options_.question));
    } else if (options_.page == Page::ExamRun || options_.page == Page::ExamReview) {
        // Screenshot helper: start the first fixed set (or a random mix) directly.
        const auto& sets = content().sets;
        std::mt19937 rng(7);
        startExam(sets.empty() ? logic::makeRandomExam(content(), 5, 60, rng) : logic::makeSetExam(sets.front()));
        if (options_.page == Page::ExamReview) finishExam();
    } else {
        go(options_.page);
    }

    while (!WindowShouldClose()) {
        frame();
        ++frameCount_;
        if (!options_.screenshot.empty() && frameCount_ == 12) {
            TakeScreenshot(options_.screenshot.c_str());
            break;
        }
    }
    saveProgress();
    th.shutdown();
    CloseWindow();
    return 0;
}

}  // namespace app
