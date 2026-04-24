package main

import "sync"

type ProcessingState struct {
	mu         sync.Mutex
	cvd        float64
	buyVolume  float64
	sellVolume float64
	tickCount  uint64
}

func (s *ProcessingState) ProcessTick(side string, volume float64) (cvd, ofi float64) {
	s.mu.Lock()
	defer s.mu.Unlock()

	switch side {
	case "buy":
		s.cvd += volume
		s.buyVolume += volume
	case "sell":
		s.cvd -= volume
		s.sellVolume += volume
	}

	s.tickCount++

	total := s.buyVolume + s.sellVolume
	if total > 0 {
		ofi = (s.buyVolume - s.sellVolume) / total
	}

	if s.tickCount%100 == 0 {
		s.buyVolume = 0
		s.sellVolume = 0
	}

	cvd = s.cvd
	return
}
