package main

import (
	"context"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"syscall"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"google.golang.org/grpc"

	pb "go-service/proto"
)

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)
	log.Println("=== Go Tick Processing Service ===")

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)

	state := &ProcessingState{}
	reg := prometheus.NewRegistry()
	metrics := NewMetrics(reg)

	metricsMux := http.NewServeMux()
	metricsMux.Handle("/metrics", promhttp.HandlerFor(reg, promhttp.HandlerOpts{}))
	metricsMux.HandleFunc("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		fmt.Fprint(w, "OK")
	})

	metricsServer := &http.Server{Addr: ":8082", Handler: metricsMux}
	go func() {
		log.Println("[METRICS] Listening on :8082")
		if err := metricsServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("[METRICS] Server error: %v", err)
		}
	}()

	go runKafkaConsumer(ctx, state, metrics)

	lis, err := net.Listen("tcp", ":50052")
	if err != nil {
		log.Fatalf("[gRPC] Listen error: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterTickServiceServer(grpcServer, &tickServer{state: state, metrics: metrics})

	log.Println("[gRPC] Listening on :50052")

	go func() {
		if err := grpcServer.Serve(lis); err != nil {
			log.Fatalf("[gRPC] Serve error: %v", err)
		}
	}()

	sig := <-sigCh
	log.Printf("[SHUTDOWN] Received %v", sig)
	cancel()

	grpcServer.GracefulStop()
	metricsServer.Shutdown(context.Background())

	log.Println("[SHUTDOWN] Stopped")
}
