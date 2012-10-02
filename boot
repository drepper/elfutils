#! /bin/bash
if ! test -f ./backends/libebl_x86_64.so;then
  echo ERROR: ./backends/libebl_x86_64.so
else
  test -d lib
  mkdir -p lib64
  test -h   lib/elfutils || ln -s ../backends   lib/elfutils
  test -h lib64/elfutils || ln -s ../backends lib64/elfutils
  export LD_LIBRARY_PATH=$PWD/backends:$PWD/libdw:$PWD/libelf:$PWD/libasm:$LD_LIBRARY_PATH
fi
echo OK
