use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use rdkafka::config::ClientConfig;
use rdkafka::consumer::{CommitMode, Consumer, StreamConsumer};
use rdkafka::message::Message;
use serde::Deserialize;
use tokio::sync::watch;
use tracing::{error, info, warn};

use crate::metrics::AppMetrics;
use crate::state::ProcessingState;

const GROUP_ID: &str = "rust-service-group";
const TOPIC: &str = "xauusd.ticks";

#[derive(Deserialize)]
struct KafkaTick {
    symbol: String,
    price: f64,
    volume: f64,
    #[allow(dead_code)]
    timestamp_ms: i64,
    side: String,
}

async fn create_consumer(broker: &str) -> StreamConsumer {
    loop {
        match ClientConfig::new()
            .set("group.id", GROUP_ID)
            .set("bootstrap.servers", broker)
            .set("auto.offset.reset", "latest")
            .set("enable.auto.commit", "true")
            .set("session.timeout.ms", "10000")
            .set("fetch.min.bytes", "1")
            .create()
        {
            Ok(c) => return c,
            Err(e) => {
                warn!("Kafka connection failed: {e}, retrying in 3s...");
                tokio::time::sleep(Duration::from_secs(3)).await;
            }
        }
    }
}

pub async fn run(
    state: Arc<Mutex<ProcessingState>>,
    metrics: Arc<AppMetrics>,
    mut shutdown: watch::Receiver<bool>,
) {
    let broker = std::env::var("KAFKA_BROKER").unwrap_or_else(|_| "kafka:9092".into());

    let consumer = create_consumer(&broker).await;
    consumer
        .subscribe(&[TOPIC])
        .expect("Failed to subscribe to topic");

    info!("Kafka consumer started (group={GROUP_ID}, topic={TOPIC})");

    loop {
        tokio::select! {
            _ = shutdown.changed() => {
                info!("Kafka consumer shutting down");
                break;
            }
            result = consumer.recv() => {
                match result {
                    Ok(msg) => process_message(&msg, &state, &metrics, &consumer),
                    Err(e) => {
                        error!("Kafka recv error: {e}");
                        tokio::time::sleep(Duration::from_millis(500)).await;
                    }
                }
            }
        }
    }
}

fn process_message(
    msg: &rdkafka::message::BorrowedMessage<'_>,
    state: &Arc<Mutex<ProcessingState>>,
    metrics: &Arc<AppMetrics>,
    consumer: &StreamConsumer,
) {
    if let Some(payload) = msg.payload() {
        if let Ok(tick) = serde_json::from_slice::<KafkaTick>(payload) {
            let start = Instant::now();
            let (cvd, ofi) = state.lock().unwrap().process_tick(&tick.side, tick.volume);
            metrics.record(cvd, ofi, start.elapsed().as_secs_f64());

            info!(
                symbol = %tick.symbol,
                price = tick.price,
                cvd,
                ofi,
                "tick processed"
            );
        }
    }
    let _ = consumer.commit_message(msg, CommitMode::Async);
}
