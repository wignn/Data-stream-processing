use std::sync::{Arc, Mutex};
use std::time::{Instant, SystemTime, UNIX_EPOCH};

use tonic::{Request, Response, Status};

use crate::metrics::AppMetrics;
use crate::proto::tick_service_server::TickService;
use crate::proto::{TickRequest, TickResponse};
use crate::state::ProcessingState;

pub struct TickServiceImpl {
    pub state: Arc<Mutex<ProcessingState>>,
    pub metrics: Arc<AppMetrics>,
}

#[tonic::async_trait]
impl TickService for TickServiceImpl {
    async fn process_tick(
        &self,
        request: Request<TickRequest>,
    ) -> Result<Response<TickResponse>, Status> {
        let start = Instant::now();
        let req = request.into_inner();

        let (cvd, ofi) = self.state.lock().unwrap().process_tick(&req.side, req.volume);

        self.metrics
            .record(cvd, ofi, start.elapsed().as_secs_f64());

        let processed_at_ms = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis() as i64;

        Ok(Response::new(TickResponse {
            cvd,
            order_flow_imbalance: ofi,
            processed_at_ms,
        }))
    }
}
