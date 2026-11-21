// runner.cpp - compiling and running a source file with the system compiler.
// POSIX: fork/exec/waitpid with process groups and rlimits (tested). Windows: CreateProcess + a job object (untested).
#include "logic/runner.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <regex>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace logic {

namespace fs = std::filesystem;

namespace {

using Clock = std::chrono::steady_clock;

// Seconds since `start`.
double since(Clock::time_point start) { return std::chrono::duration<double>(Clock::now() - start).count(); }

// ---------------------------------------------------------------------------------------------------------------------
// Compiler lookup
// ---------------------------------------------------------------------------------------------------------------------

#ifdef _WIN32
constexpr char kPathSep = ';';
const char* const kExeSuffix = ".exe";
#else
constexpr char kPathSep = ':';
const char* const kExeSuffix = "";
#endif

// Splits a PATH-style list into directories (empty parts skipped).
std::vector<std::string> splitPath(const std::string& list) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : list + std::string(1, kPathSep)) {
        if (c == kPathSep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    return out;
}

// True when `p` is a regular file the current user may execute.
bool isExecutable(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) return false;
#ifdef _WIN32
    return true;
#else
    return access(p.c_str(), X_OK) == 0;
#endif
}

// Directories that hold compilers on Windows and are often missing from PATH.
std::vector<std::string> extraCompilerDirs() {
#ifdef _WIN32
    return {"C:\\msys64\\ucrt64\\bin", "C:\\msys64\\mingw64\\bin", "C:\\msys64\\clang64\\bin", "C:\\mingw64\\bin",
            "C:\\MinGW\\bin",          "C:\\TDM-GCC-64\\bin",      "C:\\Program Files\\LLVM\\bin"};
#else
    return {};
#endif
}

// First executable called `name` in the given directories.
fs::path findIn(const std::vector<std::string>& dirs, const std::string& name) {
    for (const std::string& d : dirs) {
        fs::path p = fs::path(d) / (name + kExeSuffix);
        if (isExecutable(p)) return p;
    }
    return {};
}

// ---------------------------------------------------------------------------------------------------------------------
// Running a child process
// ---------------------------------------------------------------------------------------------------------------------

struct ProcSpec {
    fs::path exe;                    // full path of the program
    std::vector<std::string> args;   // arguments after the program name
    fs::path cwd;                    // working directory
    double timeoutSeconds = 3.0;
    size_t outputCap = 64 * 1024;
    bool limits = false;             // memory / CPU / file-size limits (the compiled program, not the compiler)
    long memoryMegabytes = 512;
    long cpuSeconds = 5;
};

struct ProcResult {
    bool started = false;
    std::string error;       // why it could not start
    bool timedOut = false;
    bool cancelled = false;
    bool signaled = false;   // killed by a signal / exception
    int signalNumber = 0;
    std::string signalText;  // name of the signal / exception
    int exitCode = 0;
    std::string output;      // stdout + stderr, capped
    bool truncated = false;
    double seconds = 0.0;
};

// Appends data to the capped output.
void appendCapped(ProcResult& r, const char* data, size_t n, size_t cap) {
    if (r.output.size() < cap) {
        const size_t room = cap - r.output.size();
        r.output.append(data, std::min(n, room));
        if (n > room) r.truncated = true;
    } else if (n > 0) {
        r.truncated = true;
    }
}

#ifndef _WIN32

// Readable name of a POSIX signal.
std::string signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (segmentation fault)";
        case SIGABRT: return "SIGABRT (aborted)";
        case SIGFPE: return "SIGFPE (arithmetic error, e.g. division by zero)";
        case SIGILL: return "SIGILL (illegal instruction)";
        case SIGBUS: return "SIGBUS (bus error)";
        case SIGXCPU: return "SIGXCPU (CPU time limit)";
        case SIGXFSZ: return "SIGXFSZ (file size limit)";
        case SIGPIPE: return "SIGPIPE (broken pipe)";
        case SIGKILL: return "SIGKILL (killed, maybe out of memory)";
        case SIGTERM: return "SIGTERM (terminated)";
        default: return "signal " + std::to_string(sig);
    }
}

// Environment of the child: the parent's, with the locale forced to C so compiler messages are English.
std::vector<std::string> childEnvironment() {
    std::vector<std::string> env;
    for (char** e = environ; e && *e; ++e) {
        const std::string s = *e;
        if (s.rfind("LC_ALL=", 0) == 0 || s.rfind("LANGUAGE=", 0) == 0 || s.rfind("LC_MESSAGES=", 0) == 0) continue;
        env.push_back(s);
    }
    env.push_back("LC_ALL=C");
    return env;
}

// Starts the program, captures its output and waits for it (kills the whole process group on timeout / cancel).
ProcResult runProcess(const ProcSpec& spec, const std::atomic<bool>* cancel) {
    ProcResult r;
    const auto start = Clock::now();

    // Everything the child needs is prepared before fork(): the child may only call async-signal-safe functions.
    const std::string exe = spec.exe.string(), cwd = spec.cwd.string();
    std::vector<std::string> argStrings{exe};
    argStrings.insert(argStrings.end(), spec.args.begin(), spec.args.end());
    std::vector<char*> argv;
    for (std::string& a : argStrings) argv.push_back(a.data());
    argv.push_back(nullptr);
    std::vector<std::string> envStrings = childEnvironment();
    std::vector<char*> envp;
    for (std::string& e : envStrings) envp.push_back(e.data());
    envp.push_back(nullptr);

    int outPipe[2] = {-1, -1}, errPipe[2] = {-1, -1};
    if (pipe(outPipe) != 0) {
        r.error = "could not create a pipe";
        return r;
    }
    if (pipe(errPipe) != 0) {
        close(outPipe[0]);
        close(outPipe[1]);
        r.error = "could not create a pipe";
        return r;
    }
    for (int fd : {outPipe[0], outPipe[1], errPipe[0], errPipe[1]}) fcntl(fd, F_SETFD, FD_CLOEXEC);

    const pid_t pid = fork();
    if (pid < 0) {
        for (int fd : {outPipe[0], outPipe[1], errPipe[0], errPipe[1]}) close(fd);
        r.error = "could not start a process";
        return r;
    }
    if (pid == 0) {
        // Child: own process group, no stdin, stdout+stderr into the pipe, limits, exec.
        setpgid(0, 0);
        int err = 0;
        if (chdir(cwd.c_str()) != 0) {
            err = errno;
        } else {
            const int nul = open("/dev/null", O_RDONLY);
            if (nul >= 0) dup2(nul, 0);
            dup2(outPipe[1], 1);
            dup2(outPipe[1], 2);
            struct rlimit rl;
            // No core dumps. A limit of exactly 1 also stops piped handlers (apport, systemd-coredump), which
            // would otherwise keep the crashed process alive for seconds; 0 does not.
            rl.rlim_cur = rl.rlim_max = 1;
            setrlimit(RLIMIT_CORE, &rl);
            if (spec.limits) {
                rl.rlim_cur = rl.rlim_max = static_cast<rlim_t>(spec.memoryMegabytes) * 1024 * 1024;
                setrlimit(RLIMIT_AS, &rl);
                rl.rlim_cur = static_cast<rlim_t>(spec.cpuSeconds);
                rl.rlim_max = static_cast<rlim_t>(spec.cpuSeconds + 1);
                setrlimit(RLIMIT_CPU, &rl);
                rl.rlim_cur = rl.rlim_max = 16 * 1024 * 1024;
                setrlimit(RLIMIT_FSIZE, &rl);
            }
            execve(exe.c_str(), argv.data(), envp.data());
            err = errno;
        }
        // exec failed: report errno through the close-on-exec pipe.
        const ssize_t w = write(errPipe[1], &err, sizeof err);
        (void)w;
        _exit(127);
    }

    setpgid(pid, pid);  // also from the parent, so the group exists whichever side runs first
    close(outPipe[1]);
    close(errPipe[1]);

    // The error pipe is closed by a successful exec, so this read ends at once; data means exec failed.
    int execErr = 0;
    const ssize_t got = read(errPipe[0], &execErr, sizeof execErr);
    close(errPipe[0]);
    if (got == static_cast<ssize_t>(sizeof execErr)) {
        int st = 0;
        waitpid(pid, &st, 0);
        close(outPipe[0]);
        r.error = std::string("could not start ") + spec.exe.filename().string() + ": " + std::strerror(execErr);
        return r;
    }
    r.started = true;

    fcntl(outPipe[0], F_SETFL, fcntl(outPipe[0], F_GETFL) | O_NONBLOCK);
    char buf[8192];
    bool pipeOpen = true, exited = false;
    int status = 0;

    // Reads what is available (at most a bounded amount per call, so timeouts keep being checked).
    auto drain = [&](int maxReads) {
        for (int i = 0; i < maxReads && pipeOpen; ++i) {
            const ssize_t n = read(outPipe[0], buf, sizeof buf);
            if (n > 0) {
                appendCapped(r, buf, static_cast<size_t>(n), spec.outputCap);
            } else if (n == 0) {
                pipeOpen = false;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno != EINTR) {
                pipeOpen = false;
            }
        }
    };

    for (;;) {
        if (pipeOpen) {
            pollfd pf{outPipe[0], POLLIN, 0};
            poll(&pf, 1, 10);
            if (pf.revents & (POLLIN | POLLHUP | POLLERR)) drain(64);
        } else {
            poll(nullptr, 0, 10);
        }
        if (waitpid(pid, &status, WNOHANG) == pid) {
            exited = true;
            break;
        }
        if (cancel && cancel->load()) {
            r.cancelled = true;
            break;
        }
        if (since(start) > spec.timeoutSeconds) {
            r.timedOut = true;
            break;
        }
    }
    if (!exited) {
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    drain(256);                // what the program wrote before it ended
    kill(-pid, SIGKILL);       // stray children of the program
    close(outPipe[0]);

    if (exited) {
        if (WIFSIGNALED(status)) {
            r.signaled = true;
            r.signalNumber = WTERMSIG(status);
            r.signalText = signalName(r.signalNumber);
        } else if (WIFEXITED(status)) {
            r.exitCode = WEXITSTATUS(status);
        }
    }
    r.seconds = since(start);
    return r;
}

#else  // _WIN32

// Converts UTF-8 to UTF-16.
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// Readable name of a Windows exception code, or "" when the code is an ordinary exit status.
std::string exceptionName(DWORD code) {
    switch (code) {
        case 0xC0000005: return "access violation (segmentation fault)";
        case 0xC00000FD: return "stack overflow";
        case 0xC0000094: return "integer division by zero";
        case 0xC000001D: return "illegal instruction";
        case 0xC0000409: return "program aborted (stack check / abort)";
        case 0xC0000017: case 0xC00000F2: return "out of memory";
        default: break;
    }
    if ((code & 0xF0000000u) != 0xC0000000u) return "";
    char hex[32];
    std::snprintf(hex, sizeof hex, "exception 0x%08lX", static_cast<unsigned long>(code));
    return hex;
}

// Starts the program inside a job object (kills children, limits memory), captures its output and waits for it.
ProcResult runProcess(const ProcSpec& spec, const std::atomic<bool>* cancel) {
    ProcResult r;
    const auto start = Clock::now();

    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        r.error = "could not create a pipe";
        return r;
    }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    std::wstring cmd = L"\"" + spec.exe.wstring() + L"\"";
    for (const std::string& a : spec.args) cmd += L" \"" + widen(a) + L"\"";

    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = wr;
    si.hStdError = wr;
    PROCESS_INFORMATION pi{};
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim{};
        lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (spec.limits) {
            lim.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
                                                     JOB_OBJECT_LIMIT_PROCESS_TIME;
            lim.ProcessMemoryLimit = static_cast<SIZE_T>(spec.memoryMegabytes) * 1024 * 1024;
            lim.BasicLimitInformation.ActiveProcessLimit = 1;
            lim.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = static_cast<LONGLONG>(spec.cpuSeconds) * 10000000LL;
        }
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &lim, sizeof lim);
    }
    const BOOL ok = CreateProcessW(spec.exe.c_str(), cmd.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                                   nullptr, spec.cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        r.error = "could not start " + spec.exe.filename().string() + " (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(rd);
        if (job) CloseHandle(job);
        return r;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    r.started = true;

    char buf[8192];
    // Reads everything currently in the pipe.
    auto drain = [&]() {
        DWORD avail = 0;
        while (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD n = 0;
            if (!ReadFile(rd, buf, std::min<DWORD>(avail, sizeof buf), &n, nullptr) || n == 0) break;
            appendCapped(r, buf, n, spec.outputCap);
        }
    };
    bool exited = false;
    for (;;) {
        drain();
        if (WaitForSingleObject(pi.hProcess, 10) == WAIT_OBJECT_0) {
            exited = true;
            break;
        }
        if (cancel && cancel->load()) {
            r.cancelled = true;
            break;
        }
        if (since(start) > spec.timeoutSeconds) {
            r.timedOut = true;
            break;
        }
    }
    if (!exited) {
        if (job) TerminateJobObject(job, 1);
        else TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 2000);
    }
    drain();
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    if (exited) {
        const std::string ex = exceptionName(code);
        if (!ex.empty()) {
            r.signaled = true;
            r.signalNumber = static_cast<int>(code);
            r.signalText = ex;
        } else {
            r.exitCode = static_cast<int>(code);
        }
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(rd);
    if (job) CloseHandle(job);  // KILL_ON_JOB_CLOSE removes stray children
    r.seconds = since(start);
    return r;
}

#endif  // _WIN32

// ---------------------------------------------------------------------------------------------------------------------
// Temporary directory
// ---------------------------------------------------------------------------------------------------------------------

// A unique directory that is deleted with the object.
class TempDir {
public:
    TempDir() {
        std::error_code ec;
        const fs::path base = fs::temp_directory_path(ec);
        if (ec) return;
        std::random_device rd;
        for (int attempt = 0; attempt < 20; ++attempt) {
            char name[40];
            std::snprintf(name, sizeof name, "learn-c-cpp-%08x%08x", rd(), rd());
            const fs::path p = base / name;
            if (fs::create_directory(p, ec) && !ec) {
                fs::permissions(p, fs::perms::owner_all, ec);
                path_ = p;
                return;
            }
        }
    }
    ~TempDir() {
        std::error_code ec;
        if (!path_.empty()) fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

// Last non-empty lines of a text (for a compile failure without a parsable message).
std::string lastLines(const std::string& text, int count) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
        if (!line.empty()) lines.push_back(line);
    std::string out;
    for (size_t i = lines.size() > static_cast<size_t>(count) ? lines.size() - static_cast<size_t>(count) : 0; i < lines.size(); ++i)
        out += (out.empty() ? "" : "\n") + lines[i];
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------------

// Number of diagnostics of one severity.
int RunResult::count(const std::string& severity) const {
    return static_cast<int>(std::count_if(diagnostics.begin(), diagnostics.end(),
                                          [&](const Diagnostic& d) { return d.severity == severity; }));
}

// Compiler search: override, then the usual names in PATH, then common Windows folders.
CompilerInfo detectCompiler(Language lang, const RunnerConfig& cfg) {
    CompilerInfo info;
    const std::string& custom = lang == Language::C ? cfg.cCompiler : cfg.cppCompiler;
    if (!custom.empty() && isExecutable(custom)) {
        info.found = true;
        info.path = custom;
        info.name = fs::path(custom).filename().string();
        return info;
    }
    std::vector<std::string> dirs;
    if (!cfg.searchPath.empty()) {
        dirs = splitPath(cfg.searchPath);
    } else {
        if (const char* path = std::getenv("PATH")) dirs = splitPath(path);
        for (const std::string& d : extraCompilerDirs()) dirs.push_back(d);
    }
    const std::vector<std::string> names = lang == Language::C ? std::vector<std::string>{"gcc", "clang", "cc"}
                                                                : std::vector<std::string>{"g++", "clang++", "c++"};
    for (const std::string& n : names) {
        const fs::path p = findIn(dirs, n);
        if (!p.empty()) {
            info.found = true;
            info.path = p.string();
            info.name = n;
            return info;
        }
    }
    return info;
}

// Parses "file:line:col: severity: text" lines; "ld"/"collect2" errors become position-less errors.
std::vector<Diagnostic> parseDiagnostics(const std::string& text) {
    static const std::regex withPos(R"(^(.+?):(\d+):(\d+):\s*(fatal error|error|warning|note):\s*(.*)$)");
    static const std::regex noCol(R"(^(.+?):(\d+):\s*(fatal error|error|warning|note):\s*(.*)$)");
    static const std::regex linker(R"(^(?:.*(?:ld|collect2)(?:\.exe)?:\s*)(?:error:\s*)?(.*(?:undefined reference|multiple definition|cannot find|ld returned).*)$)");
    std::vector<Diagnostic> out;
    std::istringstream in(text);
    std::string line;
    std::smatch m;
    while (std::getline(in, line) && out.size() < 200) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        Diagnostic d;
        if (std::regex_match(line, m, withPos)) {
            d.line = std::atoi(m[2].str().c_str());
            d.column = std::atoi(m[3].str().c_str());
            d.severity = m[4] == "fatal error" ? "error" : m[4].str();
            d.message = m[5].str();
        } else if (std::regex_match(line, m, noCol)) {
            d.line = std::atoi(m[2].str().c_str());
            d.severity = m[3] == "fatal error" ? "error" : m[3].str();
            d.message = m[4].str();
        } else if (std::regex_match(line, m, linker)) {
            d.severity = "error";
            d.message = m[1].str();
        } else {
            continue;
        }
        out.push_back(d);
    }
    return out;
}

// Name shown in the UI.
const char* verdictName(Verdict v) {
    switch (v) {
        case Verdict::CompilerMissing: return "No compiler";
        case Verdict::CompileError: return "Compile error";
        case Verdict::Timeout: return "Timeout";
        case Verdict::Crash: return "Crash";
        case Verdict::Finished: return "Finished";
        case Verdict::Cancelled: return "Cancelled";
        case Verdict::InternalError: return "Runner error";
    }
    return "";
}

// Compile error, timeout and crash give 0 for the whole exam.
bool failsWholeExam(Verdict v) { return v == Verdict::CompileError || v == Verdict::Timeout || v == Verdict::Crash; }

// Compile, then run, in a private temp directory.
RunResult runSource(Language lang, const std::string& source, const RunnerConfig& cfg, const std::atomic<bool>* cancel,
                    std::atomic<int>* phase) {
    RunResult res;
    const CompilerInfo cc = detectCompiler(lang, cfg);
    if (!cc.found) {
        res.verdict = Verdict::CompilerMissing;
        res.detail = lang == Language::C ? "No C compiler (gcc, clang or cc) was found." : "No C++ compiler (g++ or clang++) was found.";
        return res;
    }
    res.compiler = cc.name;

    TempDir dir;
    if (dir.path().empty()) {
        res.verdict = Verdict::InternalError;
        res.detail = "Could not create a temporary directory.";
        return res;
    }
    const std::string srcName = lang == Language::C ? "main.c" : "main.cpp";
    const std::string exeName = std::string("prog") + kExeSuffix;
    {
        std::ofstream f(dir.path() / srcName, std::ios::binary);
        f << source;
        if (!f) {
            res.verdict = Verdict::InternalError;
            res.detail = "Could not write the source file.";
            return res;
        }
    }

    // Compile
    if (phase) phase->store(1);
    ProcSpec cs;
    cs.exe = cc.path;
    cs.cwd = dir.path();
    cs.args = {lang == Language::C ? "-std=c11" : "-std=c++20", "-Wall", "-Wextra", "-o", exeName, srcName};
#ifdef _WIN32
    cs.args.push_back("-static");  // MinGW: no runtime DLLs needed next to the program
#endif
    if (lang == Language::C) cs.args.push_back("-lm");
    cs.timeoutSeconds = cfg.compileSeconds;
    cs.outputCap = cfg.outputCap * 4;
    const ProcResult comp = runProcess(cs, cancel);
    res.compileSeconds = comp.seconds;
    res.compilerOutput = comp.output;
    res.diagnostics = parseDiagnostics(comp.output);
    if (!comp.started) {
        res.verdict = Verdict::InternalError;
        res.detail = comp.error;
        return res;
    }
    if (comp.cancelled) {
        res.verdict = Verdict::Cancelled;
        return res;
    }
    if (comp.timedOut) {
        res.verdict = Verdict::Timeout;
        res.detail = "The compiler did not finish within " + std::to_string(static_cast<int>(cfg.compileSeconds)) + " seconds.";
        return res;
    }
    if (comp.signaled || comp.exitCode != 0 || !fs::exists(dir.path() / exeName)) {
        res.verdict = Verdict::CompileError;
        if (res.count("error") == 0) {  // e.g. the compiler itself failed: keep its last lines as the message
            Diagnostic d;
            d.severity = "error";
            d.message = lastLines(comp.output, 3);
            if (d.message.empty()) d.message = "The compiler failed (exit code " + std::to_string(comp.exitCode) + ").";
            res.diagnostics.push_back(d);
        }
        return res;
    }

    // Run
    if (phase) phase->store(2);
    ProcSpec rs;
    rs.exe = dir.path() / exeName;
    rs.cwd = dir.path();
    rs.timeoutSeconds = cfg.runSeconds;
    rs.outputCap = cfg.outputCap;
    rs.limits = true;
    rs.memoryMegabytes = cfg.memoryMegabytes;
    rs.cpuSeconds = cfg.cpuSeconds;
    const ProcResult run = runProcess(rs, cancel);
    res.runSeconds = run.seconds;
    res.output = run.output;
    res.outputTruncated = run.truncated;
    if (!run.started) {
        res.verdict = Verdict::InternalError;
        res.detail = run.error;
    } else if (run.cancelled) {
        res.verdict = Verdict::Cancelled;
    } else if (run.timedOut) {
        res.verdict = Verdict::Timeout;
        std::ostringstream d;
        d << "The program ran longer than " << cfg.runSeconds << " seconds and was stopped.";
        res.detail = d.str();
    } else if (run.signaled) {
        res.verdict = Verdict::Crash;
        res.signalName = run.signalText;
    } else {
        res.verdict = Verdict::Finished;
        res.exitCode = run.exitCode;
    }
    return res;
}

// ---------------------------------------------------------------------------------------------------------------------
// Runner (worker thread)
// ---------------------------------------------------------------------------------------------------------------------

Runner::~Runner() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
}

// Starts the worker; the previous (taken) worker is joined first.
bool Runner::start(Language lang, std::string source, const RunnerConfig& cfg) {
    if (state_ != State::Idle) return false;
    if (worker_.joinable()) worker_.join();
    cancel_ = false;
    done_ = false;
    phase_ = 1;
    state_ = State::Running;
    worker_ = std::thread([this, lang, src = std::move(source), cfg]() {
        result_ = runSource(lang, src, cfg, &cancel_, &phase_);
        phase_ = 0;
        done_ = true;
    });
    return true;
}

// Asks the worker to stop; the result arrives as Cancelled.
void Runner::cancel() { cancel_ = true; }

// Hands over the result once the worker has finished.
bool Runner::poll(RunResult& out) {
    if (state_ != State::Running || !done_.load()) return false;
    if (worker_.joinable()) worker_.join();
    out = std::move(result_);
    state_ = State::Idle;
    return true;
}

}  // namespace logic
