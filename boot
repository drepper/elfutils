#! /bin/bash
if ! test -f ./backends/libebl_x86_64.so;then
  echo ERROR: ./backends/libebl_x86_64.so
else
  export LD_LIBRARY_PATH=$PWD/backends:$PWD/libdw:$PWD/libelf:$PWD/libasm:$LD_LIBRARY_PATH
fi
echo OK
