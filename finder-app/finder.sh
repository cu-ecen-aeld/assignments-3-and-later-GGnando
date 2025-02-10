#!/bin/sh

filesdir=$1
searchstr=$2
exit_code=1

#check arguments to script
if [ $# -lt 1 ]; then
    echo "Illegal number of parameters - usauge: finder.sh filesdir searchstr" >&2
    exit $exit_code
fi

#make sure directory exist before searching
if [ ! -d "$filesdir" ]; then
    echo "$filesdir does not exist."
    exit $exit_code
fi

#search directory for string
number_of_files="$(grep -lr "$searchstr" $filesdir | wc -l)"
number_of_lines="$(grep -oEr "$searchstr" $filesdir |wc -l)"

#print results
echo "The number of files are $number_of_files and the number of matching lines are $number_of_lines"