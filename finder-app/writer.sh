#!/bin/sh

writefile=$1
writestr=$2
exit_code=1

#check arguments to script
if [ $# -lt 1 ]; then
    echo "Illegal number of parameters - usauge: writer.sh writefile writestr" >&2
    exit $exit_code
fi

#create file
install -Dv /dev/null $writefile
exit_status=$?

#check that file was able to be created
if [ $exit_status -ne 0 ]; then
  echo "failed to create file: $exit_status"
  exit $exit_code
fi

#write string to file
echo $writestr > $writefile