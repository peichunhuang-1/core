# core
gRPC and TCP topic based communication tool

# Preliminary：

1. Boost (1.73.0 or newer) 
2. gRPC (1.55.0 maybe lower version may work) 

Preliminary Installation:

# Boost: 

Download from https://www.boost.org/users/history/version_1_73_0.html \
$ cd boost_1_73_0 \
$ ./bootstrap.sh --prefix=/usr/local \
$ ./b2 install 

# gRPC:

$ export GRPC_INSTALL_DIR=$HOME/.local \
$ mkdir -p $GRPC_INSTALL_DIR \
$ export PATH="$GRPC_INSTALL_DIR/bin:$PATH" \
$ git clone --recurse-submodules -b v1.55.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc \
$ cd grpc \
$ mkdir -p cmake/build \
$ pushd cmake/build \
$ cmake -DgRPC_INSTALL=ON 
      -DgRPC_BUILD_TESTS=OFF 
      -DCMAKE_INSTALL_PREFIX=$GRPC_INSTALL_DIR 
      ../.. \
$ make -j4 \
$ make install \
$ popd 

# Package Installation:

$ export GRPC_INSTALL_DIR=$HOME/.local \
$ export PATH="$GRPC_INSTALL_DIR/bin:$PATH" \
$ cd core/src \
$ mkdir build \
$ cmake -DCMAKE_PREFIX_PATH=$GRPC_INSTALL_DIR -DCMAKE_INSTALL_PREFIX=$GRPC_INSTALL_DIR .. \
$ make -j4 \
$ sudo make install 

# Test Build:

$ cd test \
$ mkdir build && cd build \
$ cmake -DCMAKE_PREFIX_PATH=$GRPC_INSTALL_DIR ..
$ make -j4

testing: 

#open server
$ $GRPC_INSTALL_DIR/bin/NodeCore

#open client(self sending and receiving)
$ ./Test
