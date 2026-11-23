// competition_screen.h - the Competition page class shared by competition.cpp (flow, start card, countdown),
// competition_round.cpp (HUD, questions, effects) and competition_result.cpp (result and Top 10 table).
#pragma once

#include <memory>
#include <string>

#include "app/screens.h"
#include "logic/answer.h"
#include "logic/blitz.h"
#include "logic/duel.h"
#include "logic/leaderboard.h"
#include "screens/common.h"
#include "screens/question_widgets.h"

namespace app {

constexpr double kFeedbackSeconds = 0.8;  // how long the green/red feedback stays before the next question
constexpr double kCountdownSeconds = 3.0 * ui::arcade::kCountStep + ui::arcade::kGoSeconds;

// Lobby -> Countdown -> Playing -> Result (solo), or for a duel: ... Playing -> Pass -> Countdown -> Playing -> DuelResult.
enum class Phase { Lobby, Countdown, Playing, Pass, Result, DuelResult };

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
    void drawSoloStart(App& app, ui::Column& col, bool hasQuestions);
    void drawDuelStart(App& app, ui::Column& col, bool hasQuestions);
    void drawRulesTiles(ui::Column& col);
    void startDuel(App& app, std::string first, std::string second);
    void drawPass(App& app, ui::Column& col);
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
    void drawDuelResult(App& app, ui::Column& col);
    void drawTable(App& app, ui::Column& col, int highlightRank, double slideStart);

    // competition_duels.cpp: the "Recent duels" card with the head-to-head line
    void drawDuelTable(App& app, ui::Column& col, const std::string& nameA, const std::string& nameB, bool glowNewest, double slideStart);

    Phase phase_ = Phase::Lobby;
    double phaseSince_ = 0.0;  // clock time the current phase began
    std::string name_;         // text in the name field
    std::string player_;       // name of the round being played or just played
    std::string name2_;        // second player's name field (duel)
    int mode_ = 0;             // lobby choice: 0 solo, 1 duel
    std::unique_ptr<logic::Duel> duel_;  // the running or finished duel (null in solo mode)
    int turn_ = 0;             // whose round of the duel is on: 0 or 1
    std::string passText_;     // name of the player who gets the keyboard next
    std::string finishedText_; // "<name> is done"
    std::string headlineText_; // "<name> wins!" or "It's a draw!"
    std::string marginText_;   // "by 40 points"
    std::string readyText_;    // "Get ready, <name>" (built once per countdown)
    int prevBest_ = -1;        // the player's best in the table before this round (-1 = none)
    int lastMult_ = 1;         // multiplier shown last, to spot tier changes
    std::string bestText_;     // "+40 vs your best (320)" under the result score
    int bestTone_ = 0;         // 1 better than before, -1 worse, 0 neutral
    std::string rankGapText_;  // "34 points to Pro"
    std::string h2hA_, h2hB_;  // names the cached head-to-head text was built for
    size_t h2hCount_ = 0;      // ... and how many duels of the track existed then
    std::string h2hText_;      // "Illia 3 - 2 Mantas, 1 draw" (empty: no duels between the two)
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
