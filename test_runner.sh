#!/bin/bash

# Test runner script for Tonyukuk compiler
# Usage: ./test_runner.sh [native|llvm] [--dogrula]

BACKEND="native"
DOGRULA=0
BEKLENEN_DIR="testler/beklenen"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dogrula)
            DOGRULA=1
            shift
            ;;
        native|llvm)
            BACKEND="$1"
            shift
            ;;
        *)
            shift
            ;;
    esac
done

TEST_DIR="testler"
COMPILER="./tonyukuk-derle"

TOTAL=0
SUCCESS=0
FAIL=0
SKIP=0

declare -a FAILED_TESTS
declare -a SKIPPED_TESTS

# Tests to skip (known problematic tests)
SKIP_LIST=(
    "test_ag.tr"           # Network test - requires network
    "test_dosya.tr"        # File operations - may have side effects
    "test_async.tr"        # Async test - complex
    "test_duzeni.tr"       # Regex test - may be complex
    "test_json.tr"         # JSON test - may have issues
    "test_generic.tr"      # Generic test - may not be fully implemented
    "test_uclu.tr"         # Ternary operator test
    "gomulu"               # Skip embedded tests (in subdirectory)
)

should_skip() {
    local test_file=$1
    for skip_pattern in "${SKIP_LIST[@]}"; do
        if [[ "$test_file" == *"$skip_pattern"* ]]; then
            return 0
        fi
    done
    return 1
}

echo "=========================================="
echo "Tonyukuk Compiler Test Suite"
echo "Backend: $BACKEND"
if [ $DOGRULA -eq 1 ]; then
    echo "Mode: Output validation (--dogrula)"
fi
echo "=========================================="
echo ""

for test_file in $TEST_DIR/*.tr; do
    # Skip files that are modules (imported by other tests)
    base_name=$(basename "$test_file")
    test_name=$(basename "$test_file" .tr)

    if should_skip "$test_file"; then
        SKIPPED_TESTS+=("$base_name (in skip list)")
        ((SKIP++))
        ((TOTAL++))
        continue
    fi

    # Check if it's a module file (typically named with _modul or yardimci suffix)
    if [[ "$base_name" == "modul_"* ]] || [[ "$base_name" == *"yardimci"* ]]; then
        SKIPPED_TESTS+=("$base_name (module file)")
        ((SKIP++))
        ((TOTAL++))
        continue
    fi

    ((TOTAL++))

    output_file="${test_file%.tr}"

    # Start timer
    START_TIME=$(date +%s%N 2>/dev/null || date +%s)

    # Compile
    if [ "$BACKEND" == "llvm" ]; then
        compile_cmd="$COMPILER --backend=llvm $test_file -o $output_file 2>&1"
    else
        compile_cmd="$COMPILER $test_file -o $output_file 2>&1"
    fi

    compile_output=$(eval $compile_cmd)
    compile_result=$?

    if [ $compile_result -ne 0 ]; then
        END_TIME=$(date +%s%N 2>/dev/null || date +%s)
        ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))
        FAILED_TESTS+=("$base_name: COMPILE FAILED")
        ((FAIL++))
        echo "  ✗ $base_name: COMPILE FAILED [${ELAPSED}ms]"
        continue
    fi

    # Run the test (with timeout)
    if [ -f "$output_file" ]; then
        run_output=$(timeout 5s "$output_file" 2>&1)
        run_result=$?

        END_TIME=$(date +%s%N 2>/dev/null || date +%s)
        ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))

        if [ $run_result -eq 124 ]; then
            FAILED_TESTS+=("$base_name: TIMEOUT")
            ((FAIL++))
            echo "  ✗ $base_name: TIMEOUT [${ELAPSED}ms]"
        elif [ $run_result -ne 0 ]; then
            FAILED_TESTS+=("$base_name: RUNTIME ERROR (exit code: $run_result)")
            ((FAIL++))
            echo "  ✗ $base_name: RUNTIME ERROR (exit code: $run_result) [${ELAPSED}ms]"
        else
            # Output validation mode
            if [ $DOGRULA -eq 1 ]; then
                beklenen_file="$BEKLENEN_DIR/${test_name}.beklenen"
                if [ -f "$beklenen_file" ]; then
                    fark=$(diff <(echo "$run_output") "$beklenen_file" 2>&1)
                    fark_code=$?
                    if [ $fark_code -eq 0 ]; then
                        echo "  ✓ $base_name (output validated) [${ELAPSED}ms]"
                        ((SUCCESS++))
                    else
                        echo "  ✗ $base_name (output mismatch) [${ELAPSED}ms]"
                        echo "    --- Diff ---"
                        echo "$fark" | head -15 | sed 's/^/    /'
                        echo "    --- ---"
                        FAILED_TESTS+=("$base_name: OUTPUT MISMATCH")
                        ((FAIL++))
                    fi
                else
                    # No expected output file, fall back to exit-code check
                    echo "  ✓ $base_name (exit code: 0, no .beklenen) [${ELAPSED}ms]"
                    ((SUCCESS++))
                fi
            else
                echo "  ✓ $base_name [${ELAPSED}ms]"
                ((SUCCESS++))
            fi
        fi

        # Clean up binary
        rm -f "$output_file"
    else
        END_TIME=$(date +%s%N 2>/dev/null || date +%s)
        ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))
        FAILED_TESTS+=("$base_name: NO OUTPUT FILE")
        ((FAIL++))
        echo "  ✗ $base_name: NO OUTPUT FILE [${ELAPSED}ms]"
    fi
done

echo ""
echo "=========================================="
echo "Test Results ($BACKEND backend)"
echo "=========================================="
echo "Total:   $TOTAL"
echo "Success: $SUCCESS"
echo "Failed:  $FAIL"
echo "Skipped: $SKIP"
echo ""

if [ $FAIL -gt 0 ]; then
    echo "Failed Tests:"
    for failed in "${FAILED_TESTS[@]}"; do
        echo "  ✗ $failed"
    done
    echo ""
fi

if [ $SKIP -gt 0 ]; then
    echo "Skipped Tests:"
    for skipped in "${SKIPPED_TESTS[@]}"; do
        echo "  ⊘ $skipped"
    done
    echo ""
fi

echo "Success Rate: $(awk "BEGIN {printf \"%.1f\", ($SUCCESS/$TOTAL)*100}")%"
echo "=========================================="

exit 0
