// competition_screen.h - the Competition page class shared by competition.cpp (flow, start card, countdown),
// competition_round.cpp (HUD, questions, effects) and competition_result.cpp (result and Top 10 table).
#pragma once

#include <memory>
#include <string>

#include "app/screens.h"
#include "logic/answer.h"
#include "logic/blitz.h"
#include "logic/leaderboard.h"
#include "screens/common.h"
#include "screens/question_widgets.h"

namespace app {

constexpr double kFeedbackSeconds = 0.8;  // how long the green/red feedback stays before the next question
constexpr double kMaxFrameStep = 0.25;    // a stalled frame (window drag) never eats more than this from the clock
constexpr double kCountdownSeconds = 3.0 * ui::arcade::kCountStep + ui::arcade::kGoSeconds;

enum class Phase { Lobby, Countdown, Playing, Result };

// The question being shown and what the player did with it.
struct Current {
    std::string id;
    int choice = -1;
    int bugLine = 0;
    std::string text;
    bool answered = false;
    double answeredAt = 0.0;
    bool correct = false;
    int points = 0;
    int multiplier = 1;
};

class CompetitionScreen : public Screen {
public:
    void onEnter(App& app) override;
    void draw(App& app, ui::Rect area) override;

private:
    // competition.cpp: flow, start card and countdown
    void drawLobby(App& app, ui::Column& col);
    void drawRulesTiles(ui::Column& col);
    void startCountdown(App& app);
    void drawCountdown(App& app, ui::Column& col);
    void startRound(App& app);
    void backToLobby();
    void drawEffects(ui::Rect area);

    // competition_round.cpp: the round itself
    void loadNext();
    void finishRound(App& app);
    void buildResultTexts();
    void drawRound(App& app, ui::Column& col);
    void drawHud(ui::Column& col);
    void drawQuestion(App& app, ui::Column& col, const content::Exercise& ex);
    void drawPredict(App& app, ui::Column& col, const content::Exercise& ex, bool cpp);
    void answer(App& app, const content::Exercise& ex, bool correct, ui::Rect origin);
    void drawFeedback(ui::Column& col, const content::Exercise& ex);

    // competition_result.cpp: result card and the Top 10 table
    void drawResult(App& app, ui::Column& col);
    void celebrate(double t);
    void drawTable(App& app, ui::Column& col, int highlightRank, double slideStart);

    Phase phase_ = Phase::Lobby;
    double phaseSince_ = 0.0;  // clock time the current phase began
    std::string name_;         // text in the name field
    std::string player_;       // name of the round being played or just played
    std::string readyText_;    // "Get ready, <name>" (built once per countdown)
    int prevBest_ = -1;        // the player's best in the table before this round (-1 = none)
    int lastMult_ = 1;         // multiplier shown last, to spot tier changes
    std::string bestText_;     // "+40 vs your best (320)" under the result score
    int bestTone_ = 0;         // 1 better than before, -1 worse, 0 neutral
    std::string rankGapText_;  // "34 points to Pro"
    std::string clearTrack_;   // track key the open "Clear table" dialog refers to
    int celebration_ = 0;      // confetti waves already fired on the result page
    logic::BlitzRound round_;
    std::unique_ptr<logic::QuestionQueue> queue_;
    Current cur_;
    logic::Placement placement_;
    int highlight_ = 0;  // 1-based rank of the run just played (0 = none)
    int roundNo_ = 0;
    bool focusName_ = true;
    bool focusAnswer_ = false;

    // Effects of the round (all driven by the frame clock, nothing allocates per frame).
    double scoreAt_ = -10.0;   // last score change
    double comboAt_ = -10.0;   // last combo change
    double tierAt_ = -10.0;    // last multiplier change
    double msgAt_ = -10.0;     // combo message start
    const char* msg_ = nullptr;
    int msgTier_ = 1;
    double flashAt_ = -10.0;
    bool flashGood_ = true;
    ui::Confetti confetti_;
    ui::FloatTexts floats_;
};

}  // namespace app
