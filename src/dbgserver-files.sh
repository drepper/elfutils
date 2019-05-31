#! /bin/sh

DB=${DB-$HOME/.dbgserver.sqlite}

if [ $# -ne 2 ]; then
    echo "usage: $0 E /full/path/to/executables"
    echo "usage: $0 D /full/path/to/debuginfo"
    echo "database: env DB=$DB"
    exit 1
fi

TYPE=$1
DIR=$2

(find $DIR -type f -print0 | xargs -0 file | while read file description
 do
    file=`echo $file | cut -f1 -d:` # trim off :
    if expr match "$description" "ELF .*" >/dev/null
    then
        mtime=`stat --format=%Y $file`
        buildid=`eu-readelf -n $file| grep Build.ID: | awk '{print $3}' `  # or scrape $description
        echo "insert into buildids values ('$buildid', '$TYPE', datetime($mtime,'unixepoch'), 'F', '$file', NULL) on conflict (buildid,artifacttype, sourcetype, source0, source1) do nothing;"
    fi
 done) | sqlite3 -echo $DB
