// tools/conformance_runner.cpp
//
// RFC 8259 conformance verdict for JSOM's parser.
//
// Corpus: third_party/json_test_suite (nst/JSONTestSuite, MIT, (c) 2016 Nicolas Seriot).
// Convention, from the suite's own README:
//   y_*  content MUST be accepted by parsers
//   n_*  content MUST be rejected by parsers
//   i_*  parsers are free to accept or reject (the suite has NO opinion)
//   files in test_transform/ are informational only: parsers may differ.
//
// Usage:  jsom_conformance [suite-dir] [--list] [--quiet]
// Exit:   0 = every y_/n_ file agrees with the suite
//         1 = at least one disagreement (a real conformance bug)
//         2 = usage / suite not found
//
// Run BOTH parse modes: the shipped default (fidelity) judges, and
// convert_unicode_escapes=true is reported alongside, because a mode change
// must not change what is accepted or rejected.

#include <jsom/jsom.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#define JSOM_CONF_FORK 1
#else
#define JSOM_CONF_FORK 0
#endif

namespace fs = std::filesystem;

namespace {

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

enum class Outcome { accepted, rejected_invalid_argument, rejected_other, crashed };

/// Set from `--validation=numbers`. A file-scope flag rather than a parameter because the
/// outcome is produced in a forked child and every call site wants the same setting.
bool g_validate_numbers = false;

// Parse in a FORKED CHILD so an implementation that dies on a suite file cannot
// take the runner with it: dying IS a result. Exit codes carry the outcome back;
// a signal means the parser crashed.
int parse_in_child(const std::string& text, bool unicode_escapes) {
    jsom::JsonParseOptions options;
    options.convert_unicode_escapes = unicode_escapes;
    options.validate_numbers = g_validate_numbers;
    try {
        auto doc = jsom::parse_document(text, options);
        (void)doc;
        return 0; // accepted
    } catch (const std::invalid_argument&) {
        return 1; // rejected, expected error type
    } catch (const std::exception&) {
        return 2; // rejected, DIFFERENT error type
    } catch (...) {
        return 3; // rejected, non-std exception
    }
}

Outcome attempt(const std::string& text, bool unicode_escapes) {
#if JSOM_CONF_FORK
    const pid_t pid = ::fork();
    if (pid == 0) {
        ::_exit(parse_in_child(text, unicode_escapes)); // no destructors, no flush
    }
    if (pid < 0) {
        return Outcome::crashed;
    }
    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        return Outcome::crashed;
    }
    if (!WIFEXITED(status)) {
        return Outcome::crashed;
    }
    switch (WEXITSTATUS(status)) {
    case 0:
        return Outcome::accepted;
    case 1:
        return Outcome::rejected_invalid_argument;
    default:
        return Outcome::rejected_other;
    }
#else
    switch (parse_in_child(text, unicode_escapes)) {
    case 0:
        return Outcome::accepted;
    case 1:
        return Outcome::rejected_invalid_argument;
    default:
        return Outcome::rejected_other;
    }
#endif
}

const char* outcome_name(Outcome o) {
    switch (o) {
    case Outcome::accepted:
        return "accepted (suite requires rejection)";
    case Outcome::rejected_invalid_argument:
        return "rejected(invalid_argument)";
    case Outcome::crashed:
        return "CRASHED (killed by a signal)";
    default:
        return "rejected(OTHER EXCEPTION TYPE)";
    }
}

struct Failure {
    std::string name;
    char cls = '?';
    std::string detail;
};

} // namespace

int main(int argc, char** argv) {
    fs::path suite{"third_party/json_test_suite"};
    bool list = false;
    bool quiet = false;
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a{argv[i]};
        if (a == "--list") {
            list = true;
        } else if (a == "--quiet") {
            quiet = true;
        } else if (a == "--verbose") {
            verbose = true;
            std::cout << std::unitbuf;
        } else if (a == "--validation=numbers") {
            g_validate_numbers = true;
        } else {
            suite = a;
        }
    }

    const fs::path parsing = suite / "test_parsing";
    if (!fs::is_directory(parsing)) {
        std::cerr << "suite not found: " << parsing.string() << "\n";
        return 2;
    }

    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(parsing)) {
        if (e.is_regular_file()) {
            files.push_back(e.path());
        }
    }
    std::sort(files.begin(), files.end());

    int y_total = 0, y_ok = 0;
    int n_total = 0, n_ok = 0;
    int i_total = 0, i_accepted = 0;
    int other_exc = 0;
    int mode_diff = 0;
    int crashes = 0;
    std::vector<Failure> failures;

    for (const auto& f : files) {
        const std::string name = f.filename().string();
        const char cls = name.empty() ? '?' : name[0];
        const std::string text = slurp(f);
        if (text.empty() && fs::file_size(f) != 0) {
            continue;
        }
        if (verbose) {
            std::cout << "trying " << name << "\n";
        }

        const Outcome dflt = attempt(text, false);
        const Outcome uni = attempt(text, true);
        if (dflt != uni) {
            ++mode_diff;
        }
        if (dflt == Outcome::rejected_other) {
            ++other_exc;
        }

        const bool accepted = dflt == Outcome::accepted;
        const bool rejected
            = (dflt == Outcome::rejected_invalid_argument) || (dflt == Outcome::rejected_other);
        if (dflt == Outcome::crashed) {
            ++crashes;
        }
        if (cls == 'y') {
            ++y_total;
            if (accepted) {
                ++y_ok;
            } else {
                failures.push_back({name, cls, outcome_name(dflt)});
            }
        } else if (cls == 'n') {
            ++n_total;
            // A crash is NOT a rejection. Dying is never conformance, even on an
            // n_ file — that is exactly the bug this suite is meant to expose.
            if (rejected) {
                ++n_ok;
            } else {
                failures.push_back({name, cls, outcome_name(dflt)});
            }
        } else if (cls == 'i') {
            ++i_total;
            if (accepted) {
                ++i_accepted;
            }
            if (dflt == Outcome::crashed) {
                failures.push_back({name, cls, "CRASHED - a crash is never a free choice"});
            }
        }
    }

    if (!quiet) {
        std::cout << "=== RFC 8259 conformance: " << suite.string() << " ===\n";
        std::cout << "  numbers: "
                  << (g_validate_numbers ? "numbers validated (--validation=numbers)"
                                         : "lazy (default)")
                  << "\n";
        std::cout << "  y_ must accept: " << y_ok << "/" << y_total
                  << (y_ok == y_total ? "  OK" : "  <-- FAILURES") << "\n";
        std::cout << "  n_ must reject: " << n_ok << "/" << n_total
                  << (n_ok == n_total ? "  OK" : "  <-- FAILURES") << "\n";
        std::cout << "  i_ no opinion : " << i_accepted << " accepted, " << (i_total - i_accepted)
                  << " rejected, of " << i_total << " (the suite explicitly abstains)\n";
        std::cout << "  other exception types on parse failure: " << other_exc << "\n";
        std::cout << "  CRASHES (killed by a signal): " << crashes
                  << (crashes == 0 ? "" : "  <-- never acceptable") << "\n";
        std::cout << "  files where convert_unicode_escapes changed accept/reject: " << mode_diff
                  << "\n";
    }

    if (!failures.empty()) {
        std::cout << "\n--- disagreements (" << failures.size() << ") ---\n";
        for (const auto& f : failures) {
            std::cout << "  " << f.name << "  [" << f.cls << "]  " << f.detail << "\n";
        }
    }

    if (list) {
        std::cout << "\n--- i_ files (the suite has no opinion; these are YOUR decisions) ---\n";
        for (const auto& f : files) {
            const std::string name = f.filename().string();
            if (!name.empty() && name[0] == 'i') {
                const Outcome d = attempt(slurp(f), false);
                std::cout << "  " << (d == Outcome::accepted ? "accept" : "reject") << "  " << name
                          << "\n";
            }
        }
    }

    const fs::path transform = suite / "test_transform";
    if (!quiet && fs::is_directory(transform)) {
        int acc = 0, tot = 0;
        for (const auto& e : fs::directory_iterator(transform)) {
            if (!e.is_regular_file()) {
                continue;
            }
            ++tot;
            if (attempt(slurp(e.path()), false) == Outcome::accepted) {
                ++acc;
            }
        }
        std::cout << "\n  informational: " << acc << "/" << tot
                  << " of test_transform/ accepted (suite states parsers may differ)\n";
    }

    const bool disagree = (y_ok != y_total) || (n_ok != n_total) || (crashes > 0);
    return disagree ? 1 : 0;
}
