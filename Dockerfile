FROM ubuntu:24.04

RUN sed -i 's|http://archive.ubuntu.com/ubuntu|http://security.ubuntu.com/ubuntu|g' /etc/apt/sources.list.d/ubuntu.sources && \
    sed -i 's|http://us.archive.ubuntu.com/ubuntu|http://security.ubuntu.com/ubuntu|g' /etc/apt/sources.list.d/ubuntu.sources

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    nlohmann-json3-dev \
	libsqlite3-dev \
	libtag1-dev

WORKDIR /app

COPY . .

RUN cmake -S . -B build
RUN cmake --build build

CMD ["/app/build/runchart_server"]