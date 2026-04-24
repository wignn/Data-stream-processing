package main

import (
	"context"
	"encoding/json"
	"log"
	"time"

	"github.com/segmentio/kafka-go"
)

type KafkaTick struct {
	Symbol      string  `json:"symbol"`
	Price       float64 `json:"price"`
	Volume      float64 `json:"volume"`
	TimestampMs int64   `json:"timestamp_ms"`
	Side        string  `json:"side"`
}

func runKafkaConsumer(ctx context.Context, state *ProcessingState, metrics *AppMetrics) {
	broker := getEnv("KAFKA_BROKER", "kafka:9092")

	reader := kafka.NewReader(kafka.ReaderConfig{
		Brokers:        []string{broker},
		GroupID:        "go-service-group",
		Topic:          "xauusd.ticks",
		MinBytes:       1,
		MaxBytes:       10e6,
		CommitInterval: time.Second,
		StartOffset:    kafka.LastOffset,
	})
	defer reader.Close()

	log.Printf("[KAFKA] Consumer started (broker=%s)", broker)

	for {
		msg, err := reader.ReadMessage(ctx)
		if err != nil {
			if ctx.Err() != nil {
				log.Println("[KAFKA] Consumer stopped")
				return
			}
			log.Printf("[KAFKA] Read error: %v, retrying...", err)
			time.Sleep(time.Second)
			continue
		}

		var tick KafkaTick
		if err := json.Unmarshal(msg.Value, &tick); err != nil {
			log.Printf("[KAFKA] Unmarshal error: %v", err)
			continue
		}

		start := time.Now()
		cvd, ofi := state.ProcessTick(tick.Side, tick.Volume)
		metrics.Record(cvd, ofi, time.Since(start).Seconds())

		log.Printf("[TICK] %s price=%.2f cvd=%.4f ofi=%.4f", tick.Symbol, tick.Price, cvd, ofi)
	}
}
