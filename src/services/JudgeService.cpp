#include "services/JudgeService.h"
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace col {

static std::string trimRight(const std::string &s)
{
    auto end = s.find_last_not_of(" \t\r\n");
    return end == std::string::npos ? "" : s.substr(0, end + 1);
}

JudgeService &JudgeService::instance()
{
    static JudgeService inst;
    return inst;
}

JudgeService::JudgeService()
{
    problems_ = {
        {0, "A + B", "easy", 15, "", {
            {"1 2\n", "3"},
            {"0 0\n", "0"},
            {"-5 3\n", "-2"},
            {"1000000 2000000\n", "3000000"},
        }},
        {1, "Reverse String", "easy", 15, "", {
            {"hello\n", "olleh"},
            {"abcde\n", "edcba"},
            {"a\n", "a"},
            {"racecar\n", "racecar"},
        }},
        {2, "Max Value", "medium", 30, "", {
            {"5\n3 1 4 1 5\n", "5"},
            {"3\n-1 -5 -3\n", "-1"},
            {"1\n42\n", "42"},
            {"6\n10 20 30 5 15 25\n", "30"},
        }},
        {3, "Sort", "medium", 30, "", {
            {"5\n5 3 1 4 2\n", "1 2 3 4 5"},
            {"3\n3 2 1\n", "1 2 3"},
            {"4\n1 1 1 1\n", "1 1 1 1"},
            {"6\n-3 5 0 -1 2 4\n", "-3 -1 0 2 4 5"},
        }},
        {4, "Max Subarray Sum", "hard", 50, "", {
            {"8\n-2 1 -3 4 -1 2 1 -5\n", "6"},
            {"5\n-1 -2 -3 -4 -5\n", "-1"},
            {"3\n1 2 3\n", "6"},
            {"6\n-1 3 -5 4 2 -1\n", "6"},
        }},
        {5, "Count Primes", "hard", 50, "", {
            {"10\n", "4"},
            {"1\n", "0"},
            {"30\n", "10"},
            {"100\n", "25"},
        }},
    };
}

const JudgeService::Problem *JudgeService::getProblem(int id) const
{
    if (id < 0 || id >= static_cast<int>(problems_.size()))
        return nullptr;
    return &problems_[id];
}

JudgeService::Result JudgeService::judge(int problemId, const std::string &language, const std::string &code)
{
    auto *prob = getProblem(problemId);
    if (!prob)
        return {Verdict::RuntimeError, 0, 0, 0};

    if (language == "python")
        return runPython(code, prob->tests);

    return {Verdict::RuntimeError, 0, 0, 0};
}

JudgeService::Result JudgeService::runPython(const std::string &code, const std::vector<TestCase> &tests)
{
    Result result;
    result.totalTests = static_cast<int>(tests.size());
    result.passedTests = 0;
    result.runtimeMs = 0;
    result.verdict = Verdict::Accepted;

    char tmpTpl[] = "/tmp/col-judge-XXXXXX";
    char *tmpDir = mkdtemp(tmpTpl);
    if (!tmpDir) {
        result.verdict = Verdict::RuntimeError;
        return result;
    }

    std::string dir(tmpDir);
    std::string codePath = dir + "/sol.py";

    {
        std::ofstream f(codePath);
        f << code;
    }

    for (const auto &tc : tests) {
        std::string inPath = dir + "/in.txt";
        std::string outPath = dir + "/out.txt";

        {
            std::ofstream f(inPath);
            f << tc.input;
        }

        auto t0 = std::chrono::steady_clock::now();

        pid_t pid = fork();
        if (pid == 0) {
            struct rlimit rl;
            rl.rlim_cur = 3;
            rl.rlim_max = 4;
            setrlimit(RLIMIT_CPU, &rl);

            rl.rlim_cur = 256UL * 1024 * 1024;
            rl.rlim_max = 256UL * 1024 * 1024;
            setrlimit(RLIMIT_AS, &rl);

            alarm(5);

            int in_fd = open(inPath.c_str(), O_RDONLY);
            int out_fd = open(outPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            int null_fd = open("/dev/null", O_WRONLY);

            if (in_fd >= 0) { dup2(in_fd, STDIN_FILENO); close(in_fd); }
            if (out_fd >= 0) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
            if (null_fd >= 0) { dup2(null_fd, STDERR_FILENO); close(null_fd); }

            execlp("python3", "python3", codePath.c_str(), nullptr);
            _exit(1);
        }

        int status;
        waitpid(pid, &status, 0);

        auto t1 = std::chrono::steady_clock::now();
        int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
        result.runtimeMs = std::max(result.runtimeMs, ms);

        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            result.verdict = (sig == SIGXCPU || sig == SIGALRM || sig == SIGKILL)
                ? Verdict::TimeLimitExceeded : Verdict::RuntimeError;
            break;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            result.verdict = Verdict::RuntimeError;
            break;
        }

        std::string output;
        {
            std::ifstream f(outPath);
            std::ostringstream ss;
            ss << f.rdbuf();
            output = ss.str();
        }

        if (trimRight(output) != trimRight(tc.expected)) {
            result.verdict = Verdict::WrongAnswer;
            break;
        }

        result.passedTests++;
    }

    std::filesystem::remove_all(dir);
    return result;
}

}
