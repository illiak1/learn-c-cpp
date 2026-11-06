// app.cpp - window setup, main loop, sidebar navigation and screen switching.
#include "app/app.h"

#include <algorithm>
#include <cstring>

#include <cstdio>
#include <cstdlib>
#include <random>

#include "app/screens.h"
#include "content/library.h"
#include "platform/paths.h"
#include "raylib.h"

namespace app {

using namespace ui;

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

Color trackColor(Track t) { return t == Track::C ? Color{37, 99, 235, 255} : Color{139, 70, 232, 255}; }

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

void Screen::end(const Column& col) { scrollEnd(scroll_, viewport_, col.y + 32.0f); }

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

void App::setTrack(Track t) {
    if (t == track_) return;
    track_ = t;
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
void App::startSession(const std::string& title, std::vector<std::string> ids) {
    session_.title = title;
    session_.ids = std::move(ids);
    session_.pos = 0;
    session_.results.assign(session_.ids.size(), -1);
    go(Page::Exercise);
}

void App::saveProgress() {
    if (progress_.dirty()) progress_.save();
}

int App::mistakeCount() {
    return static_cast<int>(progress_.wrongExercises(trackKey()).size() + progress_.failedTasks(trackKey()).size());
}

// Records the exam result and the pass/fail state of every marked task (once per exam).
void App::commitExam() {
    if (exam_.saved || !exam_.finished) return;
    const std::string today = logic::todayString();
    const auto tasks = logic::examTasks(content(), exam_);
    bool any = exam_.fileAnswered;
    for (size_t i = 0; i < tasks.size() && i < exam_.marks.size(); ++i) {
        if (exam_.marks[i] < 0) continue;
        any = true;
        progress_.recordTask(trackKey(), tasks[i]->id, exam_.marks[i] == 1, today);
    }
    if (!any) return;
    const logic::Score sc = logic::scoreExam(content(), exam_);
    logic::ExamRecord r{today, trackKey(), exam_.title, sc.earned, sc.total, static_cast<int>(exam_.finishedAt - exam_.startedAt), exam_.fileRan};
    progress_.recordExam(r);
    exam_.saved = true;
    saveProgress();
}

// Begins an exam: stamps the start time and opens the run page.
void App::startExam(logic::Exam exam) {
    exam.startedAt = GetTime();
    exam.finished = false;
    exam.saved = false;
    exam_ = std::move(exam);
    go(Page::ExamRun);
}

// Stops the clock and opens the review.
void App::finishExam() {
    if (!exam_.finished) {
        exam_.finished = true;
        exam_.finishedAt = GetTime();
    }
    go(Page::ExamReview);
}

void App::go(Page page) {
    if (page_ == Page::ExamReview && page != Page::ExamReview) commitExam();
    page_ = page;
    screens_[page_]->onEnter(*this);
}

Rect App::contentArea() const {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    return Rect{metrics::kSidebarW, 0.0f, w - metrics::kSidebarW, h};
}

// Left navigation: logo, track toggle, page list, theme switch.
void App::drawSidebar(Rect r) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    DrawRectangleRec(r, p.surface);
    DrawLineEx(Vector2{r.x + r.width - 0.5f, r.y}, Vector2{r.x + r.width - 0.5f, r.y + r.height}, 1.0f, p.border);

    // Logo
    Rect mark{r.x + 20.0f, r.y + 22.0f, 38.0f, 38.0f};
    shadowRound(mark, 10.0f, withAlpha(p.accent, 0.3f));
    fillRound(mark, 10.0f, p.accent);
    drawCentered(Face::Bold, 17, "C++", mark, p.textOnAccent);
    drawText(Face::Bold, 18, "Exam Trainer", r.x + 70.0f, r.y + 24.0f, p.text);
    drawText(Face::Regular, 13, "C and C++ practice", r.x + 70.0f, r.y + 43.0f, p.textMuted);

    // Track toggle
    drawText(Face::Bold, 12, "TRACK", r.x + 20.0f, r.y + 82.0f, p.textMuted);
    const int sel = tabs(Rect{r.x + 16.0f, r.y + 102.0f, r.width - 32.0f, 42.0f}, {"C", "C++"}, track_ == Track::C ? 0 : 1);
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
    drawText(Face::Bold, 12, "LEARN", r.x + 20.0f, r.y + 170.0f, p.textMuted);
    float y = r.y + 192.0f;
    for (const Item& it : items) {
        Rect row{r.x + 12.0f, y, r.width - 24.0f, 42.0f};
        const bool active = sectionOf(page_) == it.section;
        const uint32_t id = cx.makeId(it.label);
        const bool hov = cx.hovered(row);
        if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
        const float h = cx.anim(id, hov ? 1.0f : 0.0f);
        if (active) fillRound(row, 10.0f, p.accentSoft);
        else if (h > 0.0f) fillRound(row, 10.0f, withAlpha(p.surfaceAlt, h));
        const Color fg = active ? p.accent : mix(p.textMuted, p.text, h);
        drawIcon(it.icon, row.x + 22.0f, row.y + row.height / 2.0f, 19.0f, fg);
        drawText(active ? Face::Bold : Face::Regular, 16, it.label, row.x + 46.0f, row.y + 11.0f, fg);
        if (it.section == Section::Mistakes) {
            const int n = mistakeCount();
            if (n > 0) badge(std::to_string(n), row.x + row.width - badgeWidth(std::to_string(n)) - 10.0f, row.y + 10.0f, WHITE, p.danger);
        }
        if (hov && cx.pressed && page_ != it.page) go(it.page);
        y += 46.0f;
    }

    // Theme switch at the bottom
    const Rect foot{r.x + 12.0f, r.y + r.height - 62.0f, r.width - 24.0f, 46.0f};
    fillRound(foot, 10.0f, p.surfaceAlt);
    const bool dark = th.mode() == ThemeMode::Dark;
    drawIcon(dark ? Icon::Moon : Icon::Sun, foot.x + 22.0f, foot.y + foot.height / 2.0f, 18.0f, p.textMuted, p.surfaceAlt);
    drawText(Face::Regular, 15, dark ? "Dark theme" : "Light theme", foot.x + 44.0f, foot.y + 13.0f, p.text);
    bool v = dark;
    if (toggleSwitch("theme-switch", Rect{foot.x + foot.width - 56.0f, foot.y + 11.0f, 42.0f, 24.0f}, v)) {
        th.setMode(v ? ThemeMode::Dark : ThemeMode::Light);
        progress_.dark = v;
        progress_.dirtyFlag();
        saveProgress();
    }
}

// One frame: input, sidebar, current page.
void App::frame() {
    Context& cx = Context::get();
    cx.beginFrame();
    const Palette& p = Theme::get().colors();
    BeginDrawing();
    ClearBackground(p.bg);
    screens_[page_]->draw(*this, contentArea());
    drawSidebar(Rect{0.0f, 0.0f, metrics::kSidebarW, static_cast<float>(GetScreenHeight())});
    cx.endFrame();
    EndDrawing();
}

int App::run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(options_.width, options_.height, "Exam Trainer - C and C++");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    Context::get().ignoreMouse = !options_.screenshot.empty();

    Theme& th = Theme::get();
    th.init((platform::resourceRoot() / "assets").string());
    progress_.load(platform::userDataDir() / "progress.txt");
    if (!options_.hasTheme) options_.dark = progress_.dark;
    if (!options_.hasTrack) options_.track = progress_.track == "cpp" ? Track::Cpp : Track::C;
    track_ = options_.track;
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
