#!/bin/bash

while [ $# -ne 0 ]; do
    FILENAME=$1
    SYMBOLNAME=$(basename $FILENAME | sed 's/[-.]/_/g')

    echo "// $FILENAME"
    echo -en "static constexpr char $SYMBOLNAME[] = \nR\"("
    cat $FILENAME
    echo -e ')";'

    shift
done
