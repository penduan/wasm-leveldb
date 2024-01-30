#!/usr/bin/env bash

set -e

message() {
    echo -e "\e[1;93mbuild hint:${*}\e[0m"
}

CWD=$(dirname $0)
CMD_PWD="$PWD/$CWD"
ROOT_PWD="$PWD"

BUILD_TYPE=Release
BUILD_COPY=false 

run_init() {
  message "Initializing"
  cp $CMD_PWD/db_impl.cc $CMD_PWD/../db/db_impl.cc
  cp $CMD_PWD/db_impl.h $CMD_PWD/../db/db_impl.h
  cp $CMD_PWD/db.h $CMD_PWD/../include/leveldb/db.h
}

run_pthread_build() {
  # pass
  echo "TODO"
}

run_cmake() { 
  local build_dir=Release
  message "Running cmake build -> $build_dir"
  # 添加/usr/include
  emcmake cmake . -B$build_dir \
    -DCMAKE_BUILD_TYPE=Release \
    -DLEVELDB_BUILD_TESTS=OFF      \
    -DLEVELDB_BUILD_BENCHMARKS=OFF \
    -DCMAKE_CXX_FLAGS=""
  cmake --build $build_dir
}

run_wasm() {
  local build_dir=Release
  local platform=$1
  local options="-L$build_dir -lembind -lleveldb -lsnappy -sALLOW_TABLE_GROWTH -sEXPORTED_RUNTIME_METHODS=['getValue'] -sMODULARIZE -I$CMD_PWD/../include $CMD_PWD/leveldb.cc"

  message "Emcc running."

  if [[ "$BUILD_TYPE" == "Debug" ]]; then
    
    options="$options -sASSERTIONS -O -g1 -lnodefs.js -lidbfs.js"
  else
    options="$options -sDYNAMIC_EXECUTION=0 -O3"
    if [[ "$platform" == "web" ]]; then
      options="$options -lidbfs.js -s ENVIRONMENT=web --pre-js ./webpre.js"
    elif [[ "$platform" == "wechat" ]]; then
      options="$options -lnodefs.js -sDYNAMIC_EXECUTION=0 -s ENVIRONMENT=node \
        --pre-js ./wechatfs.js --pre-js ./wechatpre.js"
    else
      options="$options -lnodefs.js -sENVIRONMENT=node"
    fi
  fi

  emcc $options -o $platform.mjs
}

run_replace() {
  local wechatfile=./wechat.mjs
  sed -i "s/require(.fs.)/getWeChatFS()/g" $wechatfile
  sed -i "s/require(.path.)/getPathAdapter()/g" $wechatfile
  sed -i "s/getBinaryPromise()\./getWasmFilePath()./g" $wechatfile
  sed -i "s/import.*module.;//g" $wechatfile
  sed -i "s/const require.*\meta.url);//g" $wechatfile
  sed -i "s/import.meta.url/''/g" $wechatfile
  sed -i "s/require(.url.).fileURLToPath(new URL(.\.\/.,..))/''/g" $wechatfile
}

if [ $1 == "init" ]; then
  run_init
  exit 1
fi


run_cmake

run_wasm web
run_wasm wechat
run_wasm node

run_replace
