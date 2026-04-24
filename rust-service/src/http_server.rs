use std::net::SocketAddr;

use prometheus::{Encoder, Registry, TextEncoder};
use tokio::net::TcpListener;
use tokio::sync::watch;
use tracing::{error, info};

pub async fn run(registry: Registry, mut shutdown: watch::Receiver<bool>) {
    let addr: SocketAddr = "0.0.0.0:8082".parse().unwrap();
    let listener = TcpListener::bind(addr)
        .await
        .expect("Failed to bind metrics port 8082");

    info!("Prometheus metrics at http://{addr}/metrics");

    loop {
        tokio::select! {
            _ = shutdown.changed() => {
                info!("Metrics server shutting down");
                break;
            }
            accepted = listener.accept() => {
                let (stream, _) = match accepted {
                    Ok(v) => v,
                    Err(e) => {
                        error!("Accept error: {e}");
                        continue;
                    }
                };

                let reg = registry.clone();
                tokio::spawn(handle_connection(stream, reg));
            }
        }
    }
}

async fn handle_connection(stream: tokio::net::TcpStream, registry: Registry) {
    let io = hyper_util::rt::TokioIo::new(stream);

    let svc = hyper::service::service_fn(move |req: hyper::Request<hyper::body::Incoming>| {
        let reg = registry.clone();
        async move { Ok::<_, hyper::Error>(handle_request(req, &reg)) }
    });

    let _ = hyper_util::server::conn::auto::Builder::new(hyper_util::rt::TokioExecutor::new())
        .serve_connection(io, svc)
        .await;
}

fn handle_request(
    req: hyper::Request<hyper::body::Incoming>,
    registry: &Registry,
) -> hyper::Response<http_body_util::Full<hyper::body::Bytes>> {
    let (status, content_type, body) = match req.uri().path() {
        "/metrics" => {
            let encoder = TextEncoder::new();
            let mut buf = Vec::with_capacity(4096);
            encoder.encode(&registry.gather(), &mut buf).unwrap();
            (
                200,
                "text/plain; version=0.0.4; charset=utf-8",
                buf,
            )
        }
        "/health" => (200, "text/plain; charset=utf-8", b"OK".to_vec()),
        _ => (404, "text/plain; charset=utf-8", b"Not Found".to_vec()),
    };

    hyper::Response::builder()
        .status(status)
        .header(hyper::header::CONTENT_TYPE, content_type)
        .body(http_body_util::Full::new(hyper::body::Bytes::from(body)))
        .unwrap()
}
