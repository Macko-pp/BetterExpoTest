#!/bin/sh
# The first line (#!/bin/sh) tells the computer: "run this file with the sh shell".

# Usage: ./test.sh [image]      (default image: expo:1.0)
# Runs the container with several inputs and checks the output / exit code.
#
# What kind of test is this?
#   An "end-to-end" test of the Docker image: we do NOT look inside expo.c.
#   We run the real container, give it inputs, and check two things:
#     1. the TEXT it prints          (check)
#     2. the EXIT CODE it ends with  (check_fails)
#   Every program ends with an exit code: 0 means "all went well",
#   any other number means "something went wrong".
#   (expo.c returns 1 for text that is not a number, and 2 for a negative
#   base with a fractional exponent.)

# Which image to test.
# ${1:-expo:1.0} means: "use the first thing typed after the script name;
# if nothing was typed, use expo:1.0".
#   ./test.sh              -> tests expo:1.0
#   ./test.sh expo:1.1     -> tests expo:1.1
# (The pipeline in ci.yml uses this: ./test.sh expo:1.0)
IMAGE="${1:-expo:1.0}"

# A flag we use to remember if something went wrong.
# 0 = nothing failed so far, 1 = at least one test failed.
FAILED=0

# ---------------------------------------------------------------
# check: for inputs that must WORK. It compares the printed text.
#
#   check "<base> <exponent>" "<expected output>"
#
# Careful: inside a function, $1 and $2 are the things we pass to the
# function (the inputs and the expected text), not the ones typed after
# the script name. Outside the functions, $1 was the image name.
# ---------------------------------------------------------------
check() {
  # docker run --rm  = run the container, and delete it when it stops
  # "$IMAGE"         = which image to run
  # $1               = the inputs, for example 2 10. There are NO quotes
  #                    around it on purpose: that lets the shell split
  #                    "2 10" into two separate arguments, 2 and 10.
  # 2>/dev/null      = throw away the error messages (we only want the result)
  # $(...)           = save what the program printed in the variable "out"
  out=$(docker run --rm "$IMAGE" $1 2>/dev/null)

  # = compares two texts. They must be exactly the same.
  if [ "$out" = "$2" ]; then
    echo "PASS: $1 -> $out"
  else
    # Show what we got and what we expected, so you can find the problem
    echo "FAIL: $1 -> got '$out', expected '$2'"
    FAILED=1
  fi
}

# ---------------------------------------------------------------
# check_fails: for inputs that must be REJECTED. It checks the exit code.
#
#   check_fails "<base> <exponent>"   (the program must exit with an error)
#
# A good program must say NO to bad input. Here we do not care about the
# text, only that the program fails.
# ---------------------------------------------------------------
check_fails() {
  # "if <command>" is true when the command ends with exit code 0 (success).
  # So if docker run succeeds here, the program accepted bad input: that is a bug.
  # >/dev/null 2>&1 = hide everything it prints (both normal output and errors)
  if docker run --rm "$IMAGE" $1 >/dev/null 2>&1; then
    echo "FAIL: '$1' should have failed but succeeded"
    FAILED=1
  else
    echo "PASS: '$1' failed as expected"
  fi
}

# ---------------------------------------------------------------
# The tests
# ---------------------------------------------------------------

# Normal case: the calculation must be right.
check "2 10"   "2 ^ 10 = 1024"

# Edge cases: unusual but valid inputs.
check "5 0"    "5 ^ 0 = 1"        # anything to the power 0 is 1
check "2 -1"   "2 ^ -1 = 0.5"     # a negative exponent means 1 / 2
check "9 0.5"  "9 ^ 0.5 = 3"      # an exponent of 0.5 is the square root

# Error cases: the program must refuse these.
check_fails "abc 2"      # "abc" is not a number
check_fails "-8 0.5"     # the square root of -8 is not a real number

# ---------------------------------------------------------------
# The summary
# ---------------------------------------------------------------

# -eq means "is equal to" (for numbers)
if [ "$FAILED" -eq 0 ]; then
  echo "All tests passed"
else
  echo "Some tests failed"
fi

# The exit code is how a script tells the world if it worked:
# 0 = success, anything else = failure.
# GitHub Actions reads this number. If it is not 0, the pipeline turns red.
exit $FAILED