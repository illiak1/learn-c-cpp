// test_runner.cpp - unit tests for the compile-and-run service (they need gcc/g++ or clang; without one they are skipped).
#include <chrono>
#include <filesystem>
#include <set>
#include <thread>

#include "logic/runner.h"
#include "test_main.h"

using namespace logic;

namespace {

// Short limits so the slow cases stay quick.
RunnerConfig quick() {
    RunnerConfig c;
    c.runSeconds = 1.0;
    return c;
}

bool haveCompiler(Language l) { return detectCompiler(l).found; }

// Names of the learn-c-cpp temp directories that exist now.
std::set<std::string> tempDirs() {
    std::set<std::string> out;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(std::filesystem::temp_directory_path(), ec))
        if (e.path().filename().string().rfind("learn-c-cpp-", 0) == 0) out.insert(e.path().filename().string());
    return out;
}

}  // namespace

TEST(runner_runs_a_c_program) {
    if (!haveCompiler(Language::C)) return;
    const RunResult r = runSource(Language::C, "#include <stdio.h>\nint main(void) { printf(\"hello %d\\n\", 6 * 7); return 3; }\n", quick());
    CHECK(r.verdict == Verdict::Finished);
    CHECK_EQ(r.output, std::string("hello 42\n"));
    CHECK_EQ(r.exitCode, 3);
    CHECK(!r.outputTruncated);
}

TEST(runner_runs_a_cpp_program) {
    if (!haveCompiler(Language::Cpp)) return;
    const RunResult r = runSource(Language::Cpp, "#include <iostream>\nint main() { std::cout << \"yes\" << std::endl; std::cerr << \"err\\n\"; }\n", quick());
    CHECK(r.verdict == Verdict::Finished);
    CHECK(r.output.find("yes\n") != std::string::npos);
    CHECK(r.output.find("err\n") != std::string::npos);  // stderr is captured too
    CHECK_EQ(r.exitCode, 0);
}

TEST(runner_reports_a_compile_error_with_position) {
    if (!haveCompiler(Language::C)) return;
    const RunResult r = runSource(Language::C, "#include <stdio.h>\nint main(void) {\n    int x = ;\n    return 0;\n}\n", quick());
    CHECK(r.verdict == Verdict::CompileError);
    CHECK(r.count("error") >= 1);
    bool found = false;
    for (const Diagnostic& d : r.diagnostics)
        if (d.severity == "error" && d.line == 3 && d.column > 0 && !d.message.empty()) found = true;
    CHECK(found);
    CHECK(r.output.empty());
}

TEST(runner_reports_a_link_error) {
    if (!haveCompiler(Language::C)) return;
    const RunResult r = runSource(Language::C, "int missing(void);\nint main(void) { return missing(); }\n", quick());
    CHECK(r.verdict == Verdict::CompileError);
    CHECK(r.count("error") >= 1);
}

TEST(runner_keeps_warnings_of_a_good_program) {
    if (!haveCompiler(Language::C)) return;
    const RunResult r = runSource(Language::C, "int main(void) { int unused; return 0; }\n", quick());
    CHECK(r.verdict == Verdict::Finished);
    CHECK(r.count("warning") >= 1);
}

TEST(runner_stops_an_infinite_loop) {
    if (!haveCompiler(Language::C)) return;
    const RunResult r = runSource(Language::C, "int main(void) { volatile int x = 0; for (;;) { x++; } }\n", quick());
    CHECK(r.verdict == Verdict::Timeout);
    CHECK(r.runSeconds >= 0.9);  // no upper bound: a loaded machine can delay the kill
}

TEST(runner_stops_an_infinite_loop_that_prints) {
    if (!haveCompiler(Language::C)) return;
    const RunResult r = runSource(Language::C, "#include <stdio.h>\nint main(void) { for (;;) puts(\"spam spam spam\"); }\n", quick());
    CHECK(r.verdict == Verdict::Timeout);
    CHECK(r.outputTruncated);
    CHECK(r.output.size() <= 64 * 1024);
}

TEST(runner_reports_a_null_pointer_crash) {
    if (!haveCompiler(Language::C)) return;
#ifndef _WIN32
    const RunResult r = runSource(Language::C, "int main(void) { volatile int* p = 0; return *p; }\n", quick());
    CHECK(r.verdict == Verdict::Crash);
    CHECK(r.signalName.find("SIGSEGV") != std::string::npos);
#endif
}

TEST(runner_reports_abort_in_cpp) {
    if (!haveCompiler(Language::Cpp)) return;
#ifndef _WIN32
    const RunResult r = runSource(Language::Cpp, "#include <cstdlib>\nint main() { std::abort(); }\n", quick());
    CHECK(r.verdict == Verdict::Crash);
    CHECK(r.signalName.find("SIGABRT") != std::string::npos);
#endif
}

TEST(runner_caps_huge_output) {
    if (!haveCompiler(Language::C)) return;
    // 8 MB of text, then a normal exit: Finished, output cut to the cap.
    const RunResult r = runSource(Language::C,
                                  "#include <stdio.h>\nint main(void) { for (int i = 0; i < 100000; ++i) "
                                  "puts(\"0123456789012345678901234567890123456789012345678901234567890123456789012345678\"); return 0; }\n",
                                  quick());
    CHECK(r.verdict == Verdict::Finished);
    CHECK(r.outputTruncated);
    CHECK_EQ(r.output.size(), static_cast<size_t>(64 * 1024));
    CHECK(r.output.compare(0, 10, "0123456789") == 0);
}

TEST(runner_reports_a_missing_compiler) {
    RunnerConfig c = quick();
    c.searchPath = "/nonexistent-dir-for-the-runner-test";
    CHECK(!detectCompiler(Language::C, c).found);
    CHECK(!detectCompiler(Language::Cpp, c).found);
    const RunResult r = runSource(Language::C, "int main(void) { return 0; }\n", c);
    CHECK(r.verdict == Verdict::CompilerMissing);
    CHECK(!r.detail.empty());
    CHECK(r.output.empty());
}

TEST(runner_uses_the_path_override) {
    if (!haveCompiler(Language::C)) return;
    const CompilerInfo found = detectCompiler(Language::C);
    RunnerConfig c = quick();
    c.searchPath = "/nonexistent-dir-for-the-runner-test";
    c.cCompiler = found.path;
    const CompilerInfo viaOverride = detectCompiler(Language::C, c);
    CHECK(viaOverride.found);
    CHECK_EQ(viaOverride.path, found.path);
    c.cCompiler = "/nonexistent/compiler";  // a wrong override falls back to the search path (empty here)
    CHECK(!detectCompiler(Language::C, c).found);
}

TEST(runner_limits_memory) {
    if (!haveCompiler(Language::C)) return;
#ifndef _WIN32
    RunnerConfig c = quick();
    c.memoryMegabytes = 64;
    const RunResult r = runSource(Language::C,
                                  "#include <stdio.h>\n#include <stdlib.h>\nint main(void) { void* p = malloc(1024UL * 1024 * 1024); "
                                  "printf(\"%s\\n\", p ? \"allocated\" : \"refused\"); return 0; }\n",
                                  c);
    CHECK(r.verdict == Verdict::Finished);
    CHECK_EQ(r.output, std::string("refused\n"));
#endif
}

TEST(runner_gives_the_program_no_input) {
    if (!haveCompiler(Language::C)) return;
    const RunResult r = runSource(Language::C, "#include <stdio.h>\nint main(void) { int c = getchar(); printf(\"%d\\n\", c == EOF); return 0; }\n", quick());
    CHECK(r.verdict == Verdict::Finished);
    CHECK_EQ(r.output, std::string("1\n"));
}

TEST(runner_removes_its_temp_directory) {
    if (!haveCompiler(Language::C)) return;
    const std::set<std::string> before = tempDirs();
    runSource(Language::C, "int main(void) { return 0; }\n", quick());
    runSource(Language::C, "int main(void) { for (;;) {} }\n", quick());
    runSource(Language::C, "this is not C\n", quick());
    CHECK(tempDirs() == before);
}

TEST(runner_parses_gcc_style_messages) {
    const auto d = parseDiagnostics(
        "main.c: In function 'main':\n"
        "main.c:3:13: error: expected expression before ';' token\n"
        "    3 |     int x = ;\n"
        "main.c:5:9: warning: unused variable 'y' [-Wunused-variable]\n"
        "main.c:5:9: note: declared here\n"
        "/usr/bin/ld: x.o: in function `main': main.c:(.text+0xe): undefined reference to `foo'\n"
        "collect2: error: ld returned 1 exit status\n");
    CHECK_EQ(d.size(), static_cast<size_t>(5));
    CHECK_EQ(d[0].severity, std::string("error"));
    CHECK_EQ(d[0].line, 3);
    CHECK_EQ(d[0].column, 13);
    CHECK_EQ(d[0].message, std::string("expected expression before ';' token"));
    CHECK_EQ(d[1].severity, std::string("warning"));
    CHECK_EQ(d[2].severity, std::string("note"));
    CHECK_EQ(d[3].line, 0);
    CHECK(d[3].message.find("undefined reference") != std::string::npos);
    CHECK_EQ(d[4].severity, std::string("error"));
    CHECK(parseDiagnostics("nothing to see here\n").empty());
}

TEST(runner_verdict_helpers) {
    CHECK(stopsTests(Verdict::CompileError));
    CHECK(stopsTests(Verdict::Timeout));
    CHECK(stopsTests(Verdict::Crash));
    CHECK(!stopsTests(Verdict::Finished));
    CHECK(!stopsTests(Verdict::CompilerMissing));
    CHECK(!stopsTests(Verdict::Cancelled));
    CHECK_EQ(std::string(verdictName(Verdict::CompileError)), std::string("Compile error"));
}

TEST(runner_thread_delivers_a_result) {
    if (!haveCompiler(Language::C)) return;
    Runner runner;
    RunResult r;
    CHECK(!runner.poll(r));
    CHECK(runner.start(Language::C, "#include <stdio.h>\nint main(void) { puts(\"ok\"); return 0; }\n", quick()));
    CHECK(runner.busy());
    CHECK(!runner.start(Language::C, "int main(void) { return 0; }\n", quick()));  // already running
    for (int i = 0; i < 1000 && !runner.poll(r); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(!runner.busy());
    CHECK(r.verdict == Verdict::Finished);
    CHECK_EQ(r.output, std::string("ok\n"));
    // The same runner can start again.
    CHECK(runner.start(Language::C, "int main(void) { return 1; }\n", quick()));
    for (int i = 0; i < 1000 && !runner.poll(r); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK_EQ(r.exitCode, 1);
}

TEST(runner_thread_can_be_cancelled) {
    if (!haveCompiler(Language::C)) return;
    RunnerConfig c = quick();
    c.runSeconds = 30.0;
    Runner runner;
    CHECK(runner.start(Language::C, "int main(void) { for (;;) {} }\n", c));
    // Wait until the program is running, then cancel.
    for (int i = 0; i < 500 && runner.phase() != 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK_EQ(runner.phase(), 2);
    runner.cancel();
    RunResult r;
    for (int i = 0; i < 3000 && !runner.poll(r); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(r.verdict == Verdict::Cancelled);  // well before the 30 s run limit
}

TEST(runner_destructor_cancels_a_running_job) {
    if (!haveCompiler(Language::C)) return;
    RunnerConfig c = quick();
    c.runSeconds = 30.0;
    const auto t0 = std::chrono::steady_clock::now();
    {
        Runner runner;
        runner.start(Language::C, "int main(void) { for (;;) {} }\n", c);
        for (int i = 0; i < 500 && runner.phase() != 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // The destructor cancelled the job instead of waiting for the 30 s limit.
    CHECK(std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 25.0);
}
