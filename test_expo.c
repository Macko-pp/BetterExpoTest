/* test_expo.c - all the tests for expo.c in ONE file.
 *
 * How to run (from the project folder, on Mac, Linux or WSL):
 *     gcc -o test_expo test_expo.c -lm && ./test_expo
 *
 * Every test uses the SAME example, 2 ^ 10 = 1024, so that only one thing changes
 * from test to test: WHAT is being tested and WHY. The test types answer three
 * different questions. Read them in this order:
 *
 *   1. SIZE       How much is tested at once?
 *                 unit -> integration -> system -> end to end
 *   2. TIMING     When do I run the test?
 *                 smoke (first, broad and shallow) -> sanity (after one change, narrow and quick)
 *                 -> regression (after every change, broad)
 *   3. VIEWPOINT  What, or whose need, is checked?
 *                 acceptance (the user's need), UI (what the person sees),
 *                 API (the contract with other programs), validation (the input rules)
 *   and, separate from all of these: NON-FUNCTIONAL tests check HOW WELL it works
 *   (here: security and performance).
 *
 * The output starts with TIMING because the smoke test builds the program and must run first.
 *
 * Every line of the output starts with PASS, FAIL or SKIP and the type in [ ].
 * The integration, system and API tests need Docker and the website (python3 server.py).
 * If they are not available the test is SKIPPED, not failed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

/* For the UNIT tests we need to call parse(), a private function inside expo.c.
   #include "expo.c" copies the whole file here, so we can call it.
   But expo.c has a main() and this file needs its own, and a program can only have one.
   So we rename the main of expo.c to expo_main, and then give the name main back. */
#define main expo_main
#include "expo.c"
#undef main

static int passed = 0, failed = 0, skipped = 0;

/* Prints a banner for a family of tests, and a heading for each type with its question */
static void family(const char *title) {
    printf("\n==================== %s ====================\n", title);
}
static void heading(const char *name, const char *question) {
    printf("\n-- %s: %s\n", name, question);
}

/* Prints one result line: PASS [type] name */
static void report(const char *type, const char *name, int ok, const char *detail) {
    if (ok) {
        printf("PASS [%s] %s\n", type, name);
        passed++;
    } else {
        printf("FAIL [%s] %s -> %s\n", type, name, detail);
        failed++;
    }
}

static void skip(const char *type, const char *name, const char *why) {
    printf("SKIP [%s] %s (%s)\n", type, name, why);
    skipped++;
}

/* check: is this true? */
static void check(const char *type, const char *name, int ok) {
    report(type, name, ok, "the condition is false");
}

/* check_code: two exit codes must be equal */
static void check_code(const char *type, const char *name, int got, int expected) {
    char detail[100];
    snprintf(detail, sizeof detail, "exit code %d, expected %d", got, expected);
    report(type, name, got == expected, detail);
}

/* check_text: two texts must be exactly equal */
static void check_text(const char *type, const char *name, const char *got, const char *expected) {
    char detail[700];
    snprintf(detail, sizeof detail, "got '%s', expected '%s'", got, expected);
    for (char *c = detail; *c; c++) {   /* show a new line as a space, so the message fits on one line */
        if (*c == '\n') *c = ' ';
    }
    report(type, name, strcmp(got, expected) == 0, detail);
}

/* run: runs a command like in a terminal.
   It saves what the command printed (text and errors) in "out" and returns its exit code. */
static int run(const char *command, char *out, size_t size) {
    char full[2300];
    snprintf(full, sizeof full, "%s 2>&1", command);   /* 2>&1 = also catch the error messages */
    FILE *p = popen(full, "r");
    size_t n = p ? fread(out, 1, size - 1, p) : 0;
    out[n] = '\0';
    int status = p ? pclose(p) : -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* expo: runs our compiled program, for example expo("2 10", out, sizeof out) */
static int expo(const char *args, char *out, size_t size) {
    char command[2200];
    snprintf(command, sizeof command, "./expo_test %s", args);
    return run(command, out, size);
}

/* website: asks the running website (server.py) for a path, like a browser would.
   With with_headers = 1 the reply also includes its headers (like Content-Type).
   Returns 0 if the website answered. */
static int website(const char *path, int with_headers, char *out, size_t size) {
    char command[300];
    snprintf(command, sizeof command, "curl -s %s 'http://localhost:8000%s'",
             with_headers ? "-i" : "", path);
    return run(command, out, size);
}

/* now: the time in seconds (used to measure speed) */
static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ===============================================================
   1. SIZE: how much is tested at once?
   =============================================================== */

/* UNIT: the smallest size. One function, on its own, called directly from C.
   2 ^ 10 starts with two pieces of text, "2" and "10". Does parse turn them into numbers? */
static void test_unit(void) {
    heading("UNIT", "Does ONE small function work on its own?");
    const char *type = "unit";
    double base, exponent;
    check(type, "parse reads '2' as the number 2", parse("2", &base) == 1 && base == 2);
    check(type, "parse reads '10' as the number 10", parse("10", &exponent) == 1 && exponent == 10);
    check(type, "parse refuses 'abc', it is not a number", parse("abc", &base) == 0);
}

/* INTEGRATION: two pieces joined. The Docker image carries the program inside it.
   Does the container print exactly what the program prints? */
static void test_integration(void) {
    heading("INTEGRATION", "Do TWO pieces work together? (the Docker image + the program)");
    const char *type = "integration";
    const char *name = "the container prints the same as the program for 2 ^ 10";
    char out[300], native[300], container[300];
    if (run("docker image inspect expo:1.0 > /dev/null", out, sizeof out) != 0) {
        skip(type, name, "image expo:1.0 not found: run docker build -t expo:1.0 .");
        return;
    }
    expo("2 10", native, sizeof native);
    run("docker run --rm expo:1.0 2 10", container, sizeof container);
    check_text(type, name, container, native);
}

/* SYSTEM: the whole finished application (website -> server.py -> Docker -> program),
   checked against the requirements that were written down BEFORE building it:
      R1: the user can calculate base ^ exponent.
      R2: bad input gives a clear error.
   The developers decide if the system meets the requirements. */
static void test_system(void) {
    heading("SYSTEM", "Does the WHOLE application meet the written requirements?");
    const char *type = "system";
    char out[400];
    if (website("/calculate?base=2&exponent=10", 0, out, sizeof out) != 0) {
        skip(type, "R1 and R2", "needs curl and the website: python3 server.py");
        return;
    }
    check(type, "R1: the user can calculate 2 ^ 10 and gets 1024", strstr(out, "2 ^ 10 = 1024") != NULL);
    website("/calculate?base=abc&exponent=2", 0, out, sizeof out);
    check(type, "R2: bad input ('abc') gives a clear error", strstr(out, "must be numbers") != NULL);
}

/* END TO END: the same whole application, but from the point of view of a PERSON doing
   the task they came for, from the first click to the answer, in a real browser.
   A C program cannot click, so this is a MANUAL test: you do it, with your own eyes or with bash. */
static void test_end_to_end(void) {
    heading("END TO END", "Can a PERSON do the whole task, from the first click to the answer?");
    skip("end-to-end", "open the page, press Calculate with 2 and 10, read 2 ^ 10 = 1024",
         "manual: start python3 server.py and open http://localhost:8000 in a browser");
}

/* ===============================================================
   2. TIMING: when do I run the test?
   =============================================================== */

/* SMOKE: broad and shallow. The first thing you run, on every build.
   If smoke fails there is no point in running anything else. */
static void test_smoke(int build_code, const char *build_output) {
    heading("SMOKE", "Does it even start? (broad and shallow: run this FIRST)");
    const char *type = "smoke";
    char out[300];
    /* -Wall asks the compiler to warn about suspicious code. No output = no warnings. */
    check(type, "expo.c compiles without warnings", build_code == 0 && build_output[0] == '\0');
    check_code(type, "the program starts and exits with 0 for 2 ^ 10", expo("2 10", out, sizeof out), 0);
}

/* SANITY: narrow and quick. You changed ONE thing: did that change work?
   The change we check here: README Level 3 breaks pow() into base * exponent.
   For 2 and 10 that gives 2 * 10 = 20 instead of 1024. */
static void test_sanity(void) {
    heading("SANITY", "Did my ONE change work? (narrow and quick: run it right after the change)");
    const char *type = "sanity";
    char out[300];
    expo("2 10", out, sizeof out);
    check_text(type, "after the pow change, 2 ^ 10 is 1024 and not 2 * 10 = 20", out, "2 ^ 10 = 1024\n");
}

/* REGRESSION: broad. Everything that worked before must still work.
   Run the whole table after EVERY change, even a change in a different place. */
static void test_regression(void) {
    heading("REGRESSION", "Did my change break anything that worked BEFORE? (broad: run after EVERY change)");
    const char *type = "regression";
    char out[300], name[100];

    /* a table: the inputs, and the exact text we expect */
    struct { const char *args, *expected; } known[] = {
        {"2 10",  "2 ^ 10 = 1024\n"},
        {"5 0",   "5 ^ 0 = 1\n"},       /* anything to the power 0 is 1 */
        {"2 -1",  "2 ^ -1 = 0.5\n"},    /* a negative exponent means 1 / 2 */
        {"9 0.5", "9 ^ 0.5 = 3\n"},     /* an exponent of 0.5 is the square root */
        {"-2 3",  "-2 ^ 3 = -8\n"},    /* a negative base is fine with a whole-number exponent */
    };
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) {
        expo(known[i].args, out, sizeof out);
        snprintf(name, sizeof name, "%s still gives the right answer", known[i].args);
        check_text(type, name, out, known[i].expected);
    }
}

/* ===============================================================
   3. VIEWPOINT: what, or whose need, is checked?
   =============================================================== */

/* ACCEPTANCE: does it give the USER what they need? Written as user stories.
   The user (or customer) decides if it is accepted, not the developer.
      Story A: "As a student, I can run the program, answer its questions and get my answer."
      Story B: "As a student, when I type something wrong, I am told what to fix." */
static void test_acceptance(void) {
    heading("ACCEPTANCE", "Does it give the USER what they need? (the user decides, not the developer)");
    const char *type = "acceptance";
    char out[300], sign[300];
    run("printf '2\\n10\\n' | ./expo_test", out, sizeof out);   /* the student types 2, Enter, 10, Enter */
    check(type, "story A: typing 2 and 10 gives the answer 2 ^ 10 = 1024", strstr(out, "2 ^ 10 = 1024") != NULL);

    expo("abc 2", out, sizeof out);
    expo("-8 0.5", sign, sizeof sign);
    check(type, "story B: a wrong input tells the student what to fix",
          strstr(out, "must be numbers") != NULL && strstr(sign, "whole-number") != NULL);
}

/* UI (user interface): what the person SEES and TYPES. This program has two screens:
   the questions of the command line, and the web page. (Clicking the page needs a browser.) */
static void test_ui(void) {
    heading("UI", "Is what the person SEES and TYPES clear?");
    const char *type = "ui";
    char out[300];
    run("printf '2\\n' | ./expo_test", out, sizeof out);      /* give a base, then stop */
    check(type, "the program asks for 'Enter the base:' and then 'Enter the exponent:'",
          strstr(out, "Enter the base:") != NULL && strstr(out, "Enter the exponent:") != NULL);
    check(type, "the web page has a Base box, an Exponent box and a Calculate button",
          run("grep -q '<label>Base' index.html && grep -q '<label>Exponent' index.html "
              "&& grep -q '>Calculate</button>' index.html", out, sizeof out) == 0);
}

/* API: the contract between our server and the programs that use it (our web page, or any
   other program). The contract: GET /calculate?base=..&exponent=.. answers with JSON,
   {"ok": true or false, "output": "text"}. If the shape changes, the web page breaks. */
static void test_api(void) {
    heading("API", "Does the endpoint keep its CONTRACT? (other programs depend on it)");
    const char *type = "api";
    char out[600];
    if (website("/calculate?base=2&exponent=10", 1, out, sizeof out) != 0) {
        skip(type, "the JSON contract", "needs curl and the website: python3 server.py");
        return;
    }
    check(type, "the reply is JSON (Content-Type: application/json)", strstr(out, "application/json") != NULL);
    check(type, "a good reply is {\"ok\": true, \"output\": \"2 ^ 10 = 1024\"}",
          strstr(out, "{\"ok\": true, \"output\": \"2 ^ 10 = 1024\"}") != NULL);
    website("/calculate?base=abc&exponent=2", 0, out, sizeof out);
    check(type, "an error reply has the same shape: {\"ok\": false, \"output\": ...}",
          strstr(out, "{\"ok\": false, \"output\": \"") != NULL);
}

/* VALIDATION: are the INPUT RULES enforced? Which inputs are accepted, which are refused?
   The rules: base and exponent must be numbers (otherwise exit code 1);
   a negative base needs a whole-number exponent (otherwise exit code 2). */
static void test_validation(void) {
    heading("VALIDATION", "Are the INPUT RULES enforced? (what is accepted, what is refused)");
    const char *type = "validation";
    char out[300], name[100];
    struct { const char *args; int code; } rules[] = {
        {"2 10",   0},    /* two numbers: accepted */
        {"-8 2",   0},    /* negative base with a whole exponent: accepted */
        {"abc 2",  1},    /* text instead of a number: refused */
        {"2abc 2", 1},    /* starts like a number but is not one: refused */
        {"-8 0.5", 2},    /* negative base with a fraction: refused */
    };
    for (size_t i = 0; i < sizeof rules / sizeof rules[0]; i++) {
        if (rules[i].code == 0) {
            snprintf(name, sizeof name, "'%s' is accepted", rules[i].args);
        } else {
            snprintf(name, sizeof name, "'%s' is refused with exit code %d", rules[i].args, rules[i].code);
        }
        check_code(type, name, expo(rules[i].args, out, sizeof out), rules[i].code);
    }
}

/* ===============================================================
   NON-FUNCTIONAL: not WHAT it does, but HOW WELL it does it
   =============================================================== */

/* SECURITY: can it be abused? Text that looks like a command must only be refused.
   If it ran, it would create the file "expo_pwned". We also check the container is not root. */
static void test_security(void) {
    heading("SECURITY", "Can it be ABUSED?");
    const char *type = "security";
    char out[300];
    remove("expo_pwned");
    int code1 = expo("'2; touch expo_pwned' 2", out, sizeof out);
    int code2 = expo("'$(touch expo_pwned)' 2", out, sizeof out);
    check(type, "a command hidden in the input is refused, not run",
          code1 == 1 && code2 == 1 && access("expo_pwned", F_OK) != 0);
    check(type, "the Dockerfile does not run the program as root",
          run("grep -q 'USER 65534' Dockerfile", out, sizeof out) == 0);
}

/* PERFORMANCE: is it fast enough? The limit is generous so that slow computers pass too. */
static void test_performance(void) {
    heading("PERFORMANCE", "Is it FAST ENOUGH?");
    const char *type = "performance";
    char out[300], detail[100];
    double start = now();
    for (int i = 0; i < 20; i++) expo("2 10", out, sizeof out);
    double seconds = now() - start;
    snprintf(detail, sizeof detail, "20 runs took %.2f seconds", seconds);
    report(type, "20 runs take less than 2 seconds", seconds < 2, detail);
}

/* ===============================================================
   Every C program starts at main(). We build the program, run every test, and report.
   =============================================================== */
int main(void) {
    char build_output[1000];

    /* Build the program we are going to test: expo.c -> ./expo_test */
    int build_code = run("cc -Wall -o expo_test expo.c -lm", build_output, sizeof build_output);

    family("TIMING: when do I run the test?   smoke -> sanity -> regression");
    test_smoke(build_code, build_output);
    if (build_code != 0) {              /* without the program, the other tests cannot run */
        printf("\nThe program does not compile, so the other tests were not run:\n%s", build_output);
    } else {
        test_sanity();
        test_regression();

        family("SIZE: how much is tested at once?   unit -> integration -> system -> end to end");
        test_unit();
        test_integration();
        test_system();
        test_end_to_end();

        family("VIEWPOINT: what, or whose need, is checked?   acceptance, UI, API, validation");
        test_acceptance();
        test_ui();
        test_api();
        test_validation();

        family("NON-FUNCTIONAL: HOW WELL does it work?");
        test_security();
        test_performance();
    }

    remove("expo_test");                /* clean up the files we made */
    remove("expo_pwned");

    printf("\n%d passed, %d failed, %d skipped\n", passed, failed, skipped);
    if (failed == 0) {
        printf("All tests passed\n");
    } else {
        printf("Some tests failed\n");
    }

    /* The exit code: 0 = success, anything else = failure.
       GitHub Actions reads this number. If it is not 0, the pipeline turns red. */
    return failed ? 1 : 0;
}