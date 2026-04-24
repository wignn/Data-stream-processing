mod grpc;
mod http_server;
mod kafka;
mod metrics;
pub mod proto;
mod state;

use std::net::SocketAddr;
use std::sync::{Arc, Mutex};

use prometheus::Registry;
use tokio::signal;
use tokio::sync::watch;
use tonic::transport::Server;
use tracing::info;

use grpc::TickServiceImpl;
use metrics::AppMetrics;
use proto::tick_service_server::TickServiceServer;
use state::ProcessingState;

#[tokio::main(worker_threads = 2)]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "info".into()),
        )
        .init();

    info!("=== Rust Tick Processing Service ===");

    let registry = Registry::new();
    let metrics = Arc::new(AppMetrics::new(&registry));
    let state = Arc::new(Mutex::new(ProcessingState::new()));

    let (shutdown_tx, shutdown_rx) = watch::channel(false);

    tokio::spawn(http_server::run(registry, shutdown_rx.clone()));
    tokio::spawn(kafka::run(state.clone(), metrics.clone(), shutdown_rx.clone()));

    let grpc_addr: SocketAddr = "0.0.0.0:50051".parse()?;
    info!("gRPC server listening on {grpc_addr}");

    Server::builder()
        .add_service(TickServiceServer::new(TickServiceImpl {
            state,
            metrics,
        }))
        .serve_with_shutdown(grpc_addr, async {
            signal::ctrl_c().await.ok();
            info!("Shutdown signal received");
            let _ = shutdown_tx.send(true);
        })
        .await?;

    info!("Rust service stopped");
    Ok(())
}
