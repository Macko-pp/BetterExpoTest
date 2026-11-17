#!/bin/sh
# The first line (#!/bin/sh) tells the computer: "run this file with the sh shell".

# Usage: sh test-web.sh
# The server must already be running:  python3 server.py
# Asks the website questions with curl and checks the replies.
#
# What kind of test is this?
#   An "end-to-end" test: we do NOT look inside server.py or expo.c.
#   We act like a user: send a request, read the answer, compare it with
#   what we expect. The request goes through the whole chain:
#     curl -> server.py -> docker run -> expo (the C program) -> back to curl
#   If any part of the chain is broken, a test fails.

# The address of the website. Every test adds a path to it, for example
# "/calculate?base=2&exponent=10" becomes http://localhost:8000/calculate?base=2&exponent=10
URL="http://localhost:8000"

# A flag we use to remember if something went wrong.
# 0 = nothing failed so far, 1 = at least one test failed.
FAILED=0

# ---------------------------------------------------------------
# check: a small function we write once and use for every test.
#
#   check "<name>" "<address>" "<text that must appear in the reply>"
#
# Inside a function, $1 is the first thing we pass in (the name),
# $2 is the second (the address) and $3 is the third (the expected text).
# ---------------------------------------------------------------
check() {
  # curl visits the address, like a browser would. -s means "silent":
  # no progress bar, only the reply. $(...) saves the reply in "reply".
  reply=$(curl -s "$URL$2")

  # echo prints the reply, and the | (pipe) hands it to grep.
  # grep looks for the expected text ($3) inside the reply.
  #   -q = quiet (just answer yes or no, do not print anything)
  #   -F = Fixed text: treat $3 as plain letters, not as a pattern
  #        (so characters like ^ and . are just characters)
  #   --  = "the options end here", in case the text starts with a -
  if echo "$reply" | grep -qF -- "$3"; then
    echo "PASS: $1"
  else
    # Show what we got and what we expected, so you can find the problem
    echo "FAIL: $1 -> got '$reply', expected it to contain '$3'"
    FAILED=1
  fi
}

# ---------------------------------------------------------------
# The tests. Each line is one question to the website.
# ---------------------------------------------------------------

# A "smoke test": the quickest check that the website is alive at all.
check "home page loads"       "/"                               "Exponent Calculator"

# Normal cases: the calculation must be right.
check "2 ^ 10"                "/calculate?base=2&exponent=10"   "2 ^ 10 = 1024"

# Edge cases: unusual but valid inputs. Anything to the power 0 is 1,
# and an exponent of 0.5 is the same as the square root.
check "5 ^ 0"                 "/calculate?base=5&exponent=0"    "5 ^ 0 = 1"
check "square root of 9"      "/calculate?base=9&exponent=0.5"  "9 ^ 0.5 = 3"

# Error cases: a good program must say NO to bad input.
# The website answers with JSON. "ok": false means "this did not work".
# The \" is needed to put a quote character inside a text in quotes.
check "abc is rejected"       "/calculate?base=abc&exponent=2"  "\"ok\": false"
# A negative number with a fractional exponent has no real answer
# (the square root of -8 is not a real number).
check "-8 ^ 0.5 is rejected"  "/calculate?base=-8&exponent=0.5" "\"ok\": false"

# ---------------------------------------------------------------
# The summary
# ---------------------------------------------------------------

# -eq means "is equal to" (for numbers)
if [ "$FAILED" -eq 0 ]; then
  echo "All website tests passed"
else
  echo "Some website tests failed"
fi

# The exit code is how a script tells the world if it worked:
# 0 = success, anything else = failure.
# GitHub Actions reads this number. If it is not 0, the pipeline turns red.
exit $FAILED