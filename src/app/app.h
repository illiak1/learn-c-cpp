// app.h - application shell: tracks, pages, navigation and the main loop.
#pragma once

#include <map>
#include <memory>
#include <string>

#include <array>
#include <vector>

#include "content/types.h"
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
    Track track = Track::C;
    Page page = Page::Home;
    std::string lesson;      // --lesson=ID (with --page=lesson)
    int section = 0;         // --section=N
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
    int frameCount_ = 0;
};

// Parses argv into options (--dark, --cpp, --page=<name>, --screenshot=<file>).
Options parseArgs(int argc, char** argv);

}  // namespace app
