package main

import "github.com/prometheus/client_golang/prometheus"

type AppMetrics struct {
	tickProcessedTotal            prometheus.Counter
	tickProcessingDurationSeconds *prometheus.HistogramVec
	cvdCurrent                    *prometheus.GaugeVec
	ofiCurrent                    *prometheus.GaugeVec
}

func NewMetrics(reg *prometheus.Registry) *AppMetrics {
	m := &AppMetrics{
		tickProcessedTotal: prometheus.NewCounter(prometheus.CounterOpts{
			Name: "tick_processed_total",
			Help: "Total number of ticks processed",
		}),
		tickProcessingDurationSeconds: prometheus.NewHistogramVec(prometheus.HistogramOpts{
			Name:    "tick_processing_duration_seconds",
			Help:    "Duration of tick processing in seconds",
			Buckets: []float64{0.0001, 0.0005, 0.001, 0.005, 0.01},
		}, []string{"service"}),
		cvdCurrent: prometheus.NewGaugeVec(prometheus.GaugeOpts{
			Name: "cvd_current",
			Help: "Current Cumulative Volume Delta",
		}, []string{"service"}),
		ofiCurrent: prometheus.NewGaugeVec(prometheus.GaugeOpts{
			Name: "ofi_current",
			Help: "Current Order Flow Imbalance",
		}, []string{"service"}),
	}

	reg.MustRegister(
		m.tickProcessedTotal,
		m.tickProcessingDurationSeconds,
		m.cvdCurrent,
		m.ofiCurrent,
		prometheus.NewProcessCollector(prometheus.ProcessCollectorOpts{}),
		prometheus.NewGoCollector(),
	)

	return m
}

func (m *AppMetrics) Record(cvd, ofi, elapsed float64) {
	m.tickProcessedTotal.Inc()
	m.tickProcessingDurationSeconds.WithLabelValues("go").Observe(elapsed)
	m.cvdCurrent.WithLabelValues("go").Set(cvd)
	m.ofiCurrent.WithLabelValues("go").Set(ofi)
}
