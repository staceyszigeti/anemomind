#!/bin/bash
set -e
truth="MD5 (uploads/anemologs/boat57f678e612063872e749d481/dpkg.log) = d7b92cb70c333ff82616ff660799a078"
result=$(md5 uploads/anemologs/boat57f678e612063872e749d481/dpkg.log)
if [ "$result" = "$truth" ]; then
  echo "PASSED :-)"
  exit 0
else
  echo "Failed, got '$result' but expected '$truth'"
  exit 1
fi