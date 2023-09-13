#! /bin/bash

expect_error()
{
    local prog=$1 expr=$2

    cat >$prog <<EOF
int main()
{
    $expr;
    return 0;
}
EOF
    $scriptdir/runeppic --config "$CONFIG" $prog 2>&1
    echo "Result code $?"
    rm $prog
}

tmpdir=$( mktemp -d )
trap 'rm -rf "$tmpdir"' EXIT
scriptdir=$PWD
cd "$tmpdir"
CONFIG="$scriptdir/testenv.conf"

expect_error invalid-octal.c 08
expect_error invalid-decimal.c 1a
expect_error invalid-suffix.c 8LUL
