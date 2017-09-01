FROM ubuntu:16.04
LABEL authors="xiaobo (peterwillcn@gmail.com), toonsevrin (toonsevrin@gmail.com)"




RUN echo 'APT::Install-Recommends 0;' >> /etc/apt/apt.conf.d/01norecommends \
 && echo 'APT::Install-Suggests 0;' >> /etc/apt/apt.conf.d/01norecommends \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y sudo wget net-tools ca-certificates unzip

RUN echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-4.0 main" >> /etc/apt/sources.list.d/llvm.list \
 && wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add - \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y git-core automake autoconf libtool build-essential pkg-config libtool \
    mpi-default-dev libicu-dev python-dev python3-dev libbz2-dev zlib1g-dev libssl-dev libgmp-dev \
    clang-4.0 lldb-4.0 lld-4.0 \
 && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/clang clang /usr/lib/llvm-4.0/bin/clang 400 \
  && update-alternatives --install /usr/bin/clang++ clang++ /usr/lib/llvm-4.0/bin/clang++ 400

RUN cd /tmp && wget https://cmake.org/files/v3.9/cmake-3.9.0-Linux-x86_64.sh \
   && mkdir /opt/cmake && chmod +x /tmp/cmake-3.9.0-Linux-x86_64.sh \
   && sh /tmp/cmake-3.9.0-Linux-x86_64.sh --prefix=/opt/cmake --skip-license \
   && ln -s /opt/cmake/bin/cmake /usr/local/bin

RUN cd /tmp && wget https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_64_0.tar.gz \
  && tar zxf boost_1_64_0.tar.gz \
  && cd boost_1_64_0 \
  && ./bootstrap.sh --with-toolset=clang \
  && ./b2 -a -j$(nproc) stage release -sHAVE_ICU=1 --sICU_PATH=/usr \
  && ./b2 install --prefix=/usr \
  && rm -rf /tmp/boost_1_64_0*

RUN cd /tmp && git clone https://github.com/cryptonomex/secp256k1-zkp.git \
  && cd secp256k1-zkp \
  && ./autogen.sh && ./configure && make && make install \
  && ldconfig && rm -rf /tmp/secp256k1-zkp*

RUN cd /tmp && mkdir wasm-compiler && cd wasm-compiler \
  && git clone --depth 1 --single-branch --branch release_40 https://github.com/llvm-mirror/llvm.git \
  && cd llvm/tools && git clone --depth 1 --single-branch --branch release_40 https://github.com/llvm-mirror/clang.git \
  && cd .. && mkdir build && cd build \
  && cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=/opt/wasm -DLLVM_TARGETS_TO_BUILD= -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly -DCMAKE_BUILD_TYPE=Release ../ \
  && make -j$(nproc) install && rm -rf /tmp/wasm-compiler

RUN mkdir -p /opt/eos/bin/data-dir && mkdir -p /tmp/eos/build/

COPY . /tmp/eos/

RUN cd /tmp/eos/build && cmake WASM_LLVM_CONFIG=/opt/wasm/bin/llvm-config -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/opt/eos ../ \
  && make -j$(nproc) && make install && mv ../contracts / \
  && ln -s /opt/eos/bin/eos* /usr/local/bin \
  && rm -rf /tmp/eos*

COPY Docker/config.ini genesis.json /
COPY Docker/entrypoint.sh /sbin
RUN chmod +x /sbin/entrypoint.sh
VOLUME /opt/eos/bin/data-dir
EXPOSE 9876 8888
ENTRYPOINT ["/sbin/entrypoint.sh"]
