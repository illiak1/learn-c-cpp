// runner.h - compiles and runs one source file with the system compiler (no UI, no Raylib).
// The blocking core is runSource(); Runner wraps it in a worker thread with poll() and cancel() for the UI.
#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace logic {

enum class Language { C, Cpp };

// How a run ended.
enum class Verdict {
    CompilerMissing,  // no compiler found
    CompileError,     // the compiler rejected the file (or the linker failed)
    Timeout,          // compile or run exceeded its wall-clock limit
    Crash,            // the program was killed by a signal / exception
    Finished,         // the program ended by itself (exitCode and output are valid)
    Cancelled,        // the caller cancelled the run
    InternalError,    // the runner itself failed (temp directory, fork, ...); see RunResult::detail
};

// One compiler message.
struct Diagnostic {
    std::string severity;  // "error", "warning" or "note"
    int line = 0;          // 1-based; 0 when the message has no position (linker errors)
    int column = 0;
    std::string message;
};

struct RunnerConfig {
    std::string cCompiler;         // path override for C (empty = auto-detect)
    std::string cppCompiler;       // path override for C++ (empty = auto-detect)
    std::string searchPath;        // directories to search instead of $PATH (empty = $PATH)
    double compileSeconds = 30.0;  // wall-clock limit for the compiler
    double runSeconds = 3.0;       // wall-clock limit for the program
    size_t outputCap = 64 * 1024;  // bytes of program / compiler output kept
    long memoryMegabytes = 512;    // address-space limit of the program (POSIX; job limit on Windows)
    long cpuSeconds = 5;           // CPU-time backstop for the program (POSIX)
};

struct CompilerInfo {
    bool found = false;
    std::string path;  // full path of the compiler
    std::string name;  // file name, e.g. "g++"
};

struct RunResult {
    Verdict verdict = Verdict::InternalError;
    std::vector<Diagnostic> diagnostics;  // parsed compiler messages (errors and warnings; also kept on success)
    std::string compilerOutput;           // raw compiler text (capped)
    std::string output;                   // program stdout + stderr in arrival order (capped)
    bool outputTruncated = false;         // the program printed more than the cap
    int exitCode = 0;                     // Finished: the exit status
    std::string signalName;               // Crash: e.g. "SIGSEGV (segmentation fault)"
    std::string compiler;                 // compiler used (file name)
    std::string detail;                   // short explanation for Timeout / InternalError / CompilerMissing
    double compileSeconds = 0.0;
    double runSeconds = 0.0;

    // Number of diagnostics with the given severity.
    int count(const std::string& severity) const;
};

// Finds the compiler for a language: the config override, then the usual names in the search path
// (C: gcc, clang, cc; C++: g++, clang++), then common MinGW/LLVM folders on Windows.
CompilerInfo detectCompiler(Language lang, const RunnerConfig& cfg = {});

// Parses gcc/clang output into diagnostics ("file:line:col: error: text"; linker lines without a position).
std::vector<Diagnostic> parseDiagnostics(const std::string& compilerOutput);

// Human text for a verdict, e.g. "Compile error".
const char* verdictName(Verdict v);

// True for the verdicts that give 0 for the whole exam in the real exam (compile error, timeout, crash).
bool failsWholeExam(Verdict v);

// Compiles and runs `source` in a fresh temporary directory (removed afterwards). Blocks until done.
// `cancel` (optional) is polled about every 10 ms; `phase` (optional) is set to 1 while compiling and 2 while running.
RunResult runSource(Language lang, const std::string& source, const RunnerConfig& cfg = {},
                    const std::atomic<bool>* cancel = nullptr, std::atomic<int>* phase = nullptr);

// Runs runSource() on a worker thread so the UI never blocks.
class Runner {
public:
    Runner() = default;
    ~Runner();
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;

    // Starts a run; returns false if one is already in progress.
    bool start(Language lang, std::string source, const RunnerConfig& cfg);
    // Asks the running job to stop (it ends with Verdict::Cancelled).
    void cancel();
    // True from start() until the result has been taken with poll().
    bool busy() const { return state_ != State::Idle; }
    // 0 = idle, 1 = compiling, 2 = running.
    int phase() const { return phase_.load(); }
    // Takes the finished result (once); false while still running or idle.
    bool poll(RunResult& out);

private:
    enum class State { Idle, Running, Done };
    std::thread worker_;
    std::atomic<bool> cancel_{false};
    std::atomic<int> phase_{0};
    std::atomic<bool> done_{false};
    State state_ = State::Idle;  // touched by the UI thread only
    RunResult result_;           // written by the worker before done_ is set
};

}  // namespace logic
