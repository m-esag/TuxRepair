// Minimal, dependency-free test harness for TuxRepair.
//
// No external framework (no GoogleTest/doctest/catch) — keeps the build free of
// additional dependencies, matching the project's offline-first ethos. Each
// TEST_CASE is a self-contained function returning bool (true = pass). The
// RUN_TEST macro prints PASS/FAIL and accumulates a global exit code.
//
// Usage:
//   bool test_foo() { ASSERT_EQ(1+1, 2); return true; }
//   RUN_TESTS { RUN_TEST(foo); }

#ifndef TUXREPAIR_TEST_RUNNER_H
#define TUXREPAIR_TEST_RUNNER_H

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace tuxrepair::test {

inline int& failure_count() {
    static int n = 0;
    return n;
}

inline void record_failure(std::string_view file, int line, std::string_view msg) {
    std::fprintf(stderr, "    FAIL  %.*s:%d  %.*s\n",
                 static_cast<int>(file.size()), file.data(),
                 line,
                 static_cast<int>(msg.size()), msg.data());
    ++failure_count();
}

inline std::string diff_msg(const char* lhs, const char* rhs, long long a, long long b) {
    std::string s(lhs);
    s += "(";
    s += std::to_string(a);
    s += ") != ";
    s += rhs;
    s += "(";
    s += std::to_string(b);
    s += ")";
    return s;
}

} // namespace tuxrepair::test

// --- Core assertion macros. On failure they record and bail out of the case. ---
//
// ASSERT_*  — for use inside test cases (which return bool). Bails out of the
//             whole test function on failure via `return false;`.
// CHECK_*   — for use inside non-bool helper functions. Sets a failure flag but
//             returns the result so the caller can propagate. Use as:
//                 if (!CHECK_TRUE(x)) return false;
#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            ::tuxrepair::test::record_failure(__FILE__, __LINE__, #cond); \
            return false; \
        } \
    } while (0)

#define CHECK_TRUE(cond) \
    ( (cond) ? true \
             : (::tuxrepair::test::record_failure(__FILE__, __LINE__, #cond), false) )

#define ASSERT_FALSE(cond) \
    do { \
        if ((cond)) { \
            ::tuxrepair::test::record_failure(__FILE__, __LINE__, "!(" #cond ")"); \
            return false; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va == _vb)) { \
            ::tuxrepair::test::record_failure( \
                __FILE__, __LINE__, \
                ::tuxrepair::test::diff_msg(#a, #b, \
                    static_cast<long long>(_va), static_cast<long long>(_vb)).c_str()); \
            return false; \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va != _vb)) { \
            ::tuxrepair::test::record_failure(__FILE__, __LINE__, #a " == " #b); \
            return false; \
        } \
    } while (0)

#define ASSERT_GE(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va >= _vb)) { \
            ::tuxrepair::test::record_failure(__FILE__, __LINE__, #a " < " #b); \
            return false; \
        } \
    } while (0)

// --- Test registry plumbing ---
#define RUN_TESTS \
    int main()

#define RUN_TEST(fn_name) \
    do { \
        std::printf("RUN    %s\n", #fn_name); \
        if ((fn_name)()) { \
            std::printf("    PASS\n"); \
        } else { \
            std::printf("    (failure recorded above)\n"); \
        } \
    } while (0)

#define TEST_RETURN \
    return ::tuxrepair::test::failure_count() == 0 ? 0 : 1

#endif // TUXREPAIR_TEST_RUNNER_H
