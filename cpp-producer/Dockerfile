FROM debian:bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates pkg-config \
    libssl-dev librdkafka-dev rapidjson-dev libspdlog-dev \
    libz-dev libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . --parallel $(nproc)

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libssl3 librdkafka1 librdkafka++1 libspdlog1.10 libfmt9 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/cpp-producer /usr/local/bin/cpp-producer

ENTRYPOINT ["cpp-producer"]
