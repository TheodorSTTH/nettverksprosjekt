# syntax=docker/dockerfile:1

FROM gcc:14 AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake git make zlib1g-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt ./
COPY backend ./backend
COPY shared ./shared

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
    && cmake --build build --target crdt_server --config Release

FROM debian:trixie-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates libstdc++6 zlib1g \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/build/crdt_server ./crdt_server

EXPOSE 3030
CMD ["./crdt_server"]
