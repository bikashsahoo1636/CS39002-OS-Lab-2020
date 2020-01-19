function min() {
    if [[ $1 > $2 ]] ; then
        return $2
    fi
    return $1
}

function max() {
    if [[ $1 < $2 ]] ; then
        return $2
    fi
    return $1
}

function calGCD() {
    max $1 $2
    local a=$?
    min $1 $2
    local b=$?
    # echo $a $b
    local c
    while [[ $b > 0 ]] ; do
        let c=a
        let a=b
        let b=c%b
    done
    return $a
}

if [[ 10 -le "$#" ]] ; then
    echo ERROR: enter less than 10 integers
    exit 1
fi

answer=$1
for var in "$@"
do
    # echo Calculating GCD for $var and $answer
    calGCD $answer $var
    # echo $?
    let answer=$?
    # echo $answer
done

echo GCD is $answer