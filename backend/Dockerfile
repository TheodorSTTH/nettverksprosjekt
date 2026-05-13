FROM gcc:14

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake \
        libboost-dev \
        libboost-system-dev \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target crdt_backend

EXPOSE 3030

CMD ["./build/crdt_backend"]
