package main

import (
	"context"
	"time"

	pb "go-service/proto"
)

type tickServer struct {
	pb.UnimplementedTickServiceServer
	state   *ProcessingState
	metrics *AppMetrics
}

func (s *tickServer) ProcessTick(_ context.Context, req *pb.TickRequest) (*pb.TickResponse, error) {
	start := time.Now()

	cvd, ofi := s.state.ProcessTick(req.Side, req.Volume)

	s.metrics.Record(cvd, ofi, time.Since(start).Seconds())

	return &pb.TickResponse{
		Cvd:                cvd,
		OrderFlowImbalance: ofi,
		ProcessedAtMs:      time.Now().UnixMilli(),
	}, nil
}
