#!/bin/sh
# usage: crunch folderName filename.ext

src="./uploads/$1/$2";
dst="./data/$1/$2"
echo "copying $src to $dst...";
#sleep 5
cp $src $dst;

# Please make sure that the crunching program is on the path
# either by adding its directory to the path or by
# copying the executable to a directory that is on the path.
#
# On the anemolab server, the file is located here:
# /home/jpilet/anemomind/build/src/server/nautical/nautical_processBoatLogs
nautical_processBoatLogs $dst $2

echo "done!"
echo "running node script..."
curl -i -H "Accept: application/json" -X POST -d "id=$1&filename=$2&polar=polar.txt" http://localhost:9000/api/upload/store
echo "done!"
