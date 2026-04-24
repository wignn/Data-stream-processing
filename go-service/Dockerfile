FROM golang:1.24 AS builder

RUN apt-get update && apt-get install -y protobuf-compiler && rm -rf /var/lib/apt/lists/*
RUN GOFLAGS="-mod=mod" go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.33.0
RUN GOFLAGS="-mod=mod" go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.3.0

WORKDIR /app

COPY proto/ /proto/
RUN mkdir -p proto
RUN protoc --go_out=proto --go_opt=paths=source_relative \
    --go-grpc_out=proto --go-grpc_opt=paths=source_relative \
    -I /proto /proto/tick.proto

COPY go-service/go.mod go-service/go.sum* ./
RUN go mod download || true

COPY go-service/*.go ./

RUN go mod tidy
RUN CGO_ENABLED=0 GOOS=linux go build -o go-service .

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y ca-certificates curl && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/go-service /usr/local/bin/go-service

EXPOSE 50052 8082

ENTRYPOINT ["go-service"]
