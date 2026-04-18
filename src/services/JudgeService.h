#pragma once
#include <string>
#include <vector>

namespace col {

class JudgeService {
public:
    struct TestCase {
        std::string input;
        std::string expected;
    };

    struct Problem {
        int id;
        std::string title;
        std::string difficulty;
        int damage;
        std::string descriptionHtml;
        std::vector<TestCase> tests;
    };

    enum class Verdict { Accepted, WrongAnswer, TimeLimitExceeded, RuntimeError };

    struct Result {
        Verdict verdict;
        int passedTests = 0;
        int totalTests = 0;
        int runtimeMs = 0;
    };

    static JudgeService &instance();

    const Problem *getProblem(int id) const;
    const std::vector<Problem> &problems() const { return problems_; }

    Result judge(int problemId, const std::string &language, const std::string &code);

private:
    JudgeService();
    Result runPython(const std::string &code, const std::vector<TestCase> &tests);
    std::vector<Problem> problems_;
};

}
