// app.h - application shell: tracks, pages, navigation and the main loop.
#pragma once

#include <map>
#include <memory>
#include <string>

#include <array>
#include <vector>

#include "content/types.h"
#include "logic/exam.h"
#include "logic/progress.h"
#include "ui/ui.h"

namespace app {

enum class Track { C, Cpp };

// Every page the app can show.
enum class Page { Home, Lessons, Lesson, Practice, Exercise, Exam, ExamRun, ExamReview, Mistakes, Stats };

// Sidebar entries; each page belongs to one of them.
enum class Section { Home, Lessons, Practice, Exam, Mistakes, Stats };

Section sectionOf(Page page);
const char* trackName(Track t);
// Accent colour used for the track badge.
Color trackColor(Track t);

class App;

// Base class of all pages: owns the scroll state and lays out a centred content column.
class Screen {
public:
    virtual ~Screen() = default;
    // Called each time the page is opened.
    virtual void onEnter(App&) { scroll_ = {}; }
    // Draws the page inside the content area.
    virtual void draw(App& app, ui::Rect area) = 0;

protected:
    // Starts the scroll view and returns a column centred in the area.
    ui::Column begin(ui::Rect area);
    // Ends the scroll view at the column's current y.
    void end(const ui::Column& col);

    ui::ScrollState scroll_;
    ui::Rect viewport_{};
};

// Command-line options (used for automated screenshots).
struct Options {
    bool dark = false;
    bool hasTheme = false;   // --dark/--light given on the command line
    Track track = Track::C;
    bool hasTrack = false;   // --c/--cpp given
    Page page = Page::Home;
    std::string lesson;      // --lesson=ID (with --page=lesson)
    int section = 0;         // --section=N
    int question = 0;        // --question=N (start index of the sample session)
    std::string screenshot;  // when set: save a PNG after a few frames and exit
};

class App {
public:
    explicit App(Options options);
    ~App();
    // Opens the window and runs until it is closed; returns the exit code.
    int run();

    Track track() const { return track_; }
    void setTrack(Track t);
    Page page() const { return page_; }
    // Switches page (calls onEnter of the new screen).
    void go(Page page);
    // Convenience for screens that need the window size.
    ui::Rect contentArea() const;

    // Content of the current track (both tracks are loaded at start-up).
    const content::TrackContent& content() const { return tracks_[track_ == Track::C ? 0 : 1]; }

    // Lesson reader state.
    void openLesson(const std::string& lessonId, int section = 0);
    const std::string& lessonId() const { return lessonId_; }
    int lessonSection() const { return lessonSection_; }
    void setLessonSection(int s) { lessonSection_ = s; }

    // A practice session: an ordered list of exercise ids the student works through.
    struct Session {
        std::string title;
        std::vector<std::string> ids;
        size_t pos = 0;
        std::vector<signed char> results;  // per question: -1 not answered, 0 wrong, 1 correct
    };
    void startSession(const std::string& title, std::vector<std::string> ids);
    Session& session() { return session_; }

    // The exam being taken or reviewed.
    void startExam(logic::Exam exam);
    logic::Exam& exam() { return exam_; }
    // Ends the timer and opens the review page.
    void finishExam();
    // Writes the finished exam (score, task results) to the progress file once.
    void commitExam();

    // Persistent progress (exercise history, streak, settings).
    logic::Progress& progress() { return progress_; }
    // Saves the progress file when something changed.
    void saveProgress();
    // "c" or "cpp": the key used in the progress file and content folders.
    std::string trackKey() const { return track_ == Track::C ? "c" : "cpp"; }
    // Exercises plus exam functions waiting for a retry (shown as a badge in the sidebar).
    int mistakeCount();

private:
    void frame();
    void drawSidebar(ui::Rect r);
    void registerScreens();

    Options options_;
    Track track_ = Track::C;
    Page page_ = Page::Home;
    std::map<Page, std::unique_ptr<Screen>> screens_;
    std::array<content::TrackContent, 2> tracks_;
    std::string lessonId_;
    int lessonSection_ = 0;
    Session session_;
    logic::Exam exam_;
    logic::Progress progress_;
    int frameCount_ = 0;
};

// Parses argv into options (--dark, --cpp, --page=<name>, --screenshot=<file>).
Options parseArgs(int argc, char** argv);

}  // namespace app
