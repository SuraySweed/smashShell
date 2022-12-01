#! /bin/bash
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
REDBG='\e[41m'
GREENBG='\e[42m'
BLUEBG='\e[44m'
NC='\033[0m' # No Color

TASKS=${TASKS:-8}
TESTS_INPUT=`pwd`/tests/inputs
TESTS_GLOB=$TESTS_INPUT/${1:-test_*}
TESTS_OUTPUT=`pwd`/tests/outputs
TESTS_EXPECTED=`pwd`/tests/expected
FORK_PRELOAD=`pwd`/tests/runner/preload/fork_preload.so
CLEANER="python3 `pwd`/tests/runner/output_cleaner.py"
SMASH=`pwd`/smash
RUNNER=`pwd`/tests/runner/runner
TMP_FOLDER=/tmp/smash_test
KEEP_ORIG=${KEEP_ORIG:-0}
VALGRIND=${VALGRIND:-0}
VALGRIND_PATH=`which valgrind`
VALGRIND_OK_LINE="All heap blocks were freed -- no leaks are possible"

mkdir -p $TESTS_OUTPUT
rm -rf $TMP_FOLDER
cp -r ./tests/required_folder $TMP_FOLDER
cd $TMP_FOLDER

for test in $TESTS_GLOB; do
    while jobs=`jobs -p | wc -w` && [[ $jobs -ge $TASKS ]]; do
        sleep 0.1
        jobs > /dev/null # for some reason without this line the loop is sometimes infinite...
    done
    test=$(basename -- "$test" .txt)
    echo Running test "$test"
    if [ $VALGRIND -eq 0 ] ; then 
        $RUNNER $SMASH < $TESTS_INPUT/$test.txt > $TESTS_OUTPUT/$test.out 2>$TESTS_OUTPUT/$test.err &
    else
        $RUNNER $VALGRIND_PATH --leak-check=full --show-reachable=yes --num-callers=20 \
        --track-fds=yes --log-file=$TESTS_OUTPUT/$test.valgrind --child-silent-after-fork=yes \
        $SMASH < $TESTS_INPUT/$test.txt > $TESTS_OUTPUT/$test.out 2>$TESTS_OUTPUT/$test.err &
    fi
done

echo ""
echo ""
echo "Waiting for all jobs to complete"
while jobs=`jobs | wc -l` && [[ $jobs -gt 0 ]]; do
    sleep 0.1
    jobs > /dev/null # for some reason without this line the loop is sometimes infinite...
done

do_diff()
{
    status=0
    if [ $VALGRIND -eq 0 ] ; then
        echo "#:TEST NAME:STDOUT:STDERR\n"
    else
        echo "#:TEST NAME:STDOUT:STDERR:VALGRIND\n"
    fi
    i=0
    for test in $TESTS_GLOB; do
        test=$(basename -- "$test" .txt)
        timeout 10 $CLEANER $TESTS_OUTPUT/$test.out $TESTS_OUTPUT/$test.err
        if [ $KEEP_ORIG -eq 0 ] ; then
            rm -f $TESTS_OUTPUT/$test.out $TESTS_OUTPUT/$test.err
        fi
        output_result="${YELLOW}MISSING${NC}"
        if [ -f "$TESTS_EXPECTED/$test.out.exp" ]; then
            if [ "`diff -q --strip-trailing-cr $TESTS_EXPECTED/$test.out.exp $TESTS_OUTPUT/$test.out.processed`" ]; then
                output_result="${RED}FAILED${NC}"
                if [ "$VSCODE_IPC_HOOK_CLI" ]; then
                    code --diff $TESTS_EXPECTED/$test.out.exp $TESTS_OUTPUT/$test.out.processed
                fi
                status=1
            else
                output_result="${GREEN}PASSED${NC}"
            fi
        fi
        err_result="${YELLOW}MISSING${NC}"
        if [ -f "$TESTS_EXPECTED/$test.err.exp" ]; then
            if [ "`diff -q --strip-trailing-cr $TESTS_EXPECTED/$test.err.exp $TESTS_OUTPUT/$test.err.processed`" ]; then
                err_result="${RED}FAILED${NC}"
                if [ "$VSCODE_IPC_HOOK_CLI" ]; then
                    code --diff $TESTS_EXPECTED/$test.err.exp $TESTS_OUTPUT/$test.err.processed
                fi
                status=1
            else
                err_result="${GREEN}PASSED${NC}"
            fi
        fi
        if [ $VALGRIND -ne 0 ] ; then
            if grep -q "$VALGRIND_OK_LINE" $TESTS_OUTPUT/$test.valgrind ; then
                valgrind_result=":${GREEN}PASSED${NC}"
            else
                valgrind_result=":${RED}FAILED${NC}"
                status=1
            fi
        fi
        (( i++ ))
        echo "$i:$test:$output_result:$err_result$valgrind_result\n"
    done
    return $status
}

# now do diff
output="$(do_diff)"
status=$?
echo -e $output | column -t -s ":"

if [ $status -eq 0 ]; then
    echo -e "${GREENBG}ALL PASSED!${NC}"
    exit 0
else
    echo -e "${REDBG}FAILED${NC}"
    exit 1
fi