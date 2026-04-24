import grpc from 'k6/net/grpc';
import { check, sleep } from 'k6';
import { Counter, Trend } from 'k6/metrics';

const client = new grpc.Client();
client.load(['../proto'], 'tick.proto');

const target = __ENV.TARGET || 'localhost:50051';
const mode = __ENV.MODE || 'fixed';

const grpcErrors = new Counter('grpc_errors');
const tickLatency = new Trend('tick_latency_ms', true);

const sides = ['buy', 'sell'];

export const options = (() => {
  if (mode === 'sustained') {
    return {
      scenarios: {
        sustained_load: {
          executor: 'ramping-vus',
          startVUs: 0,
          stages: [
            { duration: '10s', target: 10 },
            { duration: '60s', target: 50 },
            { duration: '10s', target: 0 },
          ],
        },
      },
      thresholds: {
        checks: ['rate>0.99'],
        grpc_req_duration: ['p(95)<500', 'p(99)<1000'],
      },
    };
  }

  return {
    vus: Number(__ENV.VUS || 50),
    iterations: Number(__ENV.ITERATIONS || 10000),
    thresholds: {
      checks: ['rate>0.99'],
      grpc_req_duration: ['p(95)<500', 'p(99)<1000'],
    },
  };
})();

let connected = false;

export default function () {
  if (!connected) {
    client.connect(target, {
      plaintext: true,
      timeout: '5s',
    });
    connected = true;
  }

  const now = Date.now();
  const side = sides[Math.floor(Math.random() * 2)];
  const price = 2345.67 + (Math.random() - 0.5) * 10;
  const volume = 0.1 + Math.random() * 5.0;

  const response = client.invoke('tick.TickService/ProcessTick', {
    symbol: 'XAUUSD',
    price: price,
    volume: volume,
    timestamp_ms: now,
    side: side,
  });

  const ok = check(response, {
    'grpc status is OK': (r) => r && r.status === grpc.StatusOK,
    'response has cvd': (r) => r && r.message && typeof r.message.cvd === 'number',
    'response has ofi': (r) => {
      if (!r || !r.message) return false;
      const ofi = r.message.orderFlowImbalance ?? r.message.order_flow_imbalance;
      return typeof ofi === 'number';
    },
  });

  if (!ok) {
    grpcErrors.add(1);
  }

  if (response && response.message) {
    const latency = Date.now() - now;
    tickLatency.add(latency);
  }
}

export function teardown() {
  if (connected) {
    client.close();
  }
}
