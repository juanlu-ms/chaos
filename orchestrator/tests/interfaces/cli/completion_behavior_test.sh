#!/usr/bin/env bash
#
# Behavioural test for the generated bash completion script.
#
# The C++ unit tests assert on the text the generator emits; this one loads that
# text into a real bash, drives _chaos_completion the way the shell does, and
# checks what actually lands in COMPREPLY.
#
# Usage: completion_behavior_test.sh <path-to-chaos-binary>

set -uo pipefail

CHAOS="${1:?usage: $0 <path-to-chaos-binary>}"

workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT

export CHAOS_HISTORY_DIR="$workdir/history"
if ! "$CHAOS" completion bash >"$workdir/chaos.bash"; then
    echo "FAIL - could not generate the completion script"
    exit 1
fi

# shellcheck source=/dev/null
source "$workdir/chaos.bash"

mkdir -p "$workdir/fixtures/subdir"
: >"$workdir/fixtures/plain.json"
: >"$workdir/fixtures/two words.json"
: >"$workdir/fixtures/notes.txt"
cd "$workdir/fixtures" || exit 1

failures=0

# Renders COMPREPLY unambiguously: <one><two> shows the split that a bare
# "${COMPREPLY[*]}" would hide.
reply_of() {
    COMP_WORDS=("$@")
    COMP_CWORD=$((${#COMP_WORDS[@]} - 1))
    COMPREPLY=()
    _chaos_completion
    if [[ ${#COMPREPLY[@]} -eq 0 ]]; then
        printf '<empty>'
    else
        printf '<%s>' "${COMPREPLY[@]}"
    fi
}

expect() {
    local description=$1 expected=$2
    shift 2
    local actual
    actual=$(reply_of "$@")
    if [[ "$actual" == "$expected" ]]; then
        echo "ok   - $description"
    else
        echo "FAIL - $description"
        echo "         words:    $*"
        echo "         expected: $expected"
        echo "         actual:   $actual"
        failures=$((failures + 1))
    fi
}

# The regression this file exists for: a candidate containing a space must stay
# one candidate. COMPREPLY=($(compgen ...)) yields <two><words.json> here.
expect "a manifest with spaces stays a single candidate" \
    "<two words.json>" chaos run "tw"

# ArgKind::ManifestPath filters to *.json; notes.txt must not be offered.
expect "run offers .json manifests" "<plain.json>" chaos run "pl"
expect "run hides non-manifest files" "<empty>" chaos run "no"

# ArgKind::FilePath does not filter, so the same prefix now resolves.
expect "--output accepts any file, not just manifests" \
    "<notes.txt>" chaos run --output "no"

# Word lists come straight from kCommands / kGlobalFlags, in table order.
expect "bare invocation offers every visible command" \
    "<list><stop><kill><run><history><serve><help>" chaos ""
expect "a leading dash switches to global options" \
    "<--verbose><--quiet><--log-level><--no-color><--help>" chaos "-"
expect "a value-taking global flag offers its choices" \
    "<trace><debug><info><warn><error><critical><off>" chaos --log-level ""

# positionalFirstOnly: the literal is offered in the first slot and nowhere else.
expect "history offers clear in the first slot" "<clear>" chaos history ""
expect "history offers nothing in later slots" "<empty>" chaos history abc123 ""
expect "history switches to options after a dash" "<--json>" chaos history "-"

# Container ids need a reachable Docker daemon, so only the shape is asserted:
# past the first slot the command must stop suggesting ids either way.
expect "stop suggests nothing past the first slot" "<empty>" chaos stop abc123 ""

# A flag whose value is free-form text must suggest nothing rather than fall
# through and offer the flag itself back.
expect "a free-form flag value suppresses completion" "<empty>" chaos serve --port ""

# The subcommand finder must skip a global flag together with its value.
expect "a global flag and its value do not hide the subcommand" \
    "<clear>" chaos --log-level info history ""

# Hidden commands still complete once typed in full.
expect "the hidden completion command still completes its shell" \
    "<bash>" chaos completion ""

# Sourcing the script must register the function, not just define it. Everything
# above calls _chaos_completion directly, so nothing else exercises `complete -F`.
if complete -p chaos 2>/dev/null | grep -q '_chaos_completion'; then
    echo "ok   - sourcing the script registers the completion function"
else
    echo "FAIL - chaos has no completion registered after sourcing the script"
    failures=$((failures + 1))
fi

# The script must leave the invoking shell exactly as it found it.
before=$(shopt -p extglob)
reply_of chaos run "pl" >/dev/null
after=$(shopt -p extglob)
if [[ "$before" == "$after" ]]; then
    echo "ok   - completing a manifest leaves shell options untouched"
else
    echo "FAIL - completing a manifest changed extglob: '$before' -> '$after'"
    failures=$((failures + 1))
fi

echo
if [[ $failures -eq 0 ]]; then
    echo "All completion behaviour checks passed."
    exit 0
fi
echo "$failures completion behaviour check(s) failed."
exit 1
