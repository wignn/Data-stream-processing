FROM rust:1.95-bookworm AS builder

RUN apt-get update && apt-get install -y protobuf-compiler cmake && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY proto/ /app/proto/
COPY rust-service/Cargo.toml rust-service/Cargo.lock* ./
COPY rust-service/build.rs ./

RUN mkdir src && echo "fn main() {}" > src/main.rs
RUN cargo build --release 2>/dev/null || true

COPY rust-service/src/ src/
RUN touch src/main.rs
RUN cargo build --release

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y ca-certificates curl && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/target/release/rust-service /usr/local/bin/rust-service

EXPOSE 50051 9091

ENTRYPOINT ["rust-service"]
