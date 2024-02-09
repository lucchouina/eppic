#! /bin/sh

passed=0
failed=0
total=0
for f in "$@" ; do
    base="${f%.out}"
    if diff -u "$base".expect "$f"
    then
	passed=$[ $passed + 1 ]
    else
	failed=$[ $failed + 1 ]
    fi
    rm -f "$base".diff
done

echo "============================================================================"
echo "# TOTAL: $#"
echo "# PASS:  $passed"
echo "# FAIL:  $failed"

exit $failed
