use prometheus::{
    GaugeVec, HistogramOpts, HistogramVec, IntCounter, Opts, Registry,
    process_collector::ProcessCollector,
};

pub struct AppMetrics {
    tick_processed_total: IntCounter,
    tick_processing_duration_seconds: HistogramVec,
    cvd_current: GaugeVec,
    ofi_current: GaugeVec,
}

impl AppMetrics {
    pub fn new(registry: &Registry) -> Self {
        let tick_processed_total =
            IntCounter::with_opts(Opts::new("tick_processed_total", "Total ticks processed"))
                .expect("metric creation");

        let tick_processing_duration_seconds = HistogramVec::new(
            HistogramOpts::new(
                "tick_processing_duration_seconds",
                "Tick processing duration",
            )
            .buckets(vec![0.0001, 0.0005, 0.001, 0.005, 0.01]),
            &["service"],
        )
        .expect("metric creation");

        let cvd_current = GaugeVec::new(
            Opts::new("cvd_current", "Current Cumulative Volume Delta"),
            &["service"],
        )
        .expect("metric creation");

        let ofi_current = GaugeVec::new(
            Opts::new("ofi_current", "Current Order Flow Imbalance"),
            &["service"],
        )
        .expect("metric creation");

        let process_collector = ProcessCollector::for_self();

        registry
            .register(Box::new(tick_processed_total.clone()))
            .expect("register tick_processed_total");
        registry
            .register(Box::new(tick_processing_duration_seconds.clone()))
            .expect("register tick_processing_duration_seconds");
        registry
            .register(Box::new(cvd_current.clone()))
            .expect("register cvd_current");
        registry
            .register(Box::new(ofi_current.clone()))
            .expect("register ofi_current");
        registry
            .register(Box::new(process_collector))
            .expect("register process_collector");

        Self {
            tick_processed_total,
            tick_processing_duration_seconds,
            cvd_current,
            ofi_current,
        }
    }

    pub fn record(&self, cvd: f64, ofi: f64, elapsed_secs: f64) {
        self.tick_processed_total.inc();
        self.tick_processing_duration_seconds
            .with_label_values(&["rust"])
            .observe(elapsed_secs);
        self.cvd_current.with_label_values(&["rust"]).set(cvd);
        self.ofi_current.with_label_values(&["rust"]).set(ofi);
    }
}
