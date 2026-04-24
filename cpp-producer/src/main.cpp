#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <algorithm>

#include <spdlog/spdlog.h>
#include <ixwebsocket/IXWebSocket.h>

#include "config.hpp"
#include "kafka_producer.hpp"
#include "tick_parser.hpp"

static std::atomic<bool>     g_running{true};
static std::atomic<uint64_t> g_ticks_published{0};
static std::atomic<uint64_t> g_unparsed_messages{0};
static std::atomic<uint64_t> g_trace_seq{0};

static std::string next_trace() {
    return "cpp-producer-" + std::to_string(g_trace_seq.fetch_add(1, std::memory_order_relaxed) + 1);
}

static std::string mask_api_key(const std::string& key) {
    if (key.size() <= 8) return "****";
    return key.substr(0, 4) + "..." + key.substr(key.size() - 4);
}

static void signal_handler(int sig) {
    spdlog::info("Received signal {}, shutting down...", sig);
    g_running.store(false, std::memory_order_release);
}

static void heartbeat_loop(ix::WebSocket& ws, int interval_sec) {
    while (g_running.load(std::memory_order_acquire)) {
        for (int i = 0; i < interval_sec * 10 && g_running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!g_running.load()) break;

        if (ws.getReadyState() == ix::ReadyState::Open) {
            ws.send(std::string("{\"code\":10010,\"trace\":\"") + next_trace() + "\"}");
            spdlog::debug("Heartbeat sent");
        }
    }
}

static void stats_loop(const KafkaTickProducer& kafka) {
    while (g_running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (!g_running.load()) break;

        spdlog::info("[STATS] published={} delivered={} errors={}",
                     g_ticks_published.load(),
                     kafka.delivered_count(),
                     kafka.error_count());
    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    config::AppConfig cfg;
    try {
        cfg = config::AppConfig::from_env();
    } catch (const std::exception& e) {
        spdlog::error("Configuration error: {}", e.what());
        return 1;
    }

    KafkaTickProducer kafka(cfg.kafka_broker, cfg.kafka_topic);
    if (!kafka.wait_for_broker(cfg.kafka_wait_timeout_sec)) {
        spdlog::error("Cannot reach Kafka broker, exiting");
        return 1;
    }

    std::string ws_url = "wss://data.infoway.io/ws?business=common&apikey=" + cfg.infoway_api_key;
    spdlog::info("Connecting to Infoway WS (business=common, api_key_len={}, api_key_mask={})",
                 cfg.infoway_api_key.size(), mask_api_key(cfg.infoway_api_key));

    ix::WebSocket ws;
    ws.setUrl(ws_url);
    ws.setPingInterval(45);
    ws.enableAutomaticReconnection();
    ws.setMaxWaitBetweenReconnectionRetries(cfg.reconnect_delay_ms);

    ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                spdlog::info("WebSocket connected");
                ws.send(std::string("{\"code\":10000,\"trace\":\"") + next_trace() + "\",\"data\":{\"codes\":\"XAUUSD\"}}");
                spdlog::info("Subscribed to XAUUSD");
                break;

            case ix::WebSocketMessageType::Close:
                spdlog::warn("WebSocket closed (code={}, reason={})",
                             msg->closeInfo.code, msg->closeInfo.reason);
                break;

            case ix::WebSocketMessageType::Error:
                spdlog::error("WebSocket error: {} (retries={})",
                              msg->errorInfo.reason, msg->errorInfo.retries);
                break;

            case ix::WebSocketMessageType::Message: {
                auto ticks = TickParser::parse(msg->str);
                if (ticks.empty()) {
                    auto n = g_unparsed_messages.fetch_add(1, std::memory_order_relaxed) + 1;
                    if (n <= 5) {
                        const size_t preview_len = std::min<size_t>(msg->str.size(), 240);
                        spdlog::warn("Unparsed WS payload #{}: {}", n, msg->str.substr(0, preview_len));
                    }
                }
                for (const auto& tick : ticks) {
                    std::string json = tick.to_json();
                    if (kafka.produce(tick.symbol, json)) {
                        g_ticks_published.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                break;
            }

            default:
                break;
        }
    });

    std::thread heartbeat_thread(heartbeat_loop, std::ref(ws), cfg.heartbeat_interval_sec);
    std::thread stats_thread(stats_loop, std::cref(kafka));

    ws.start();
    spdlog::info("WebSocket client started");

    while (g_running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ws.stop();
    heartbeat_thread.join();
    stats_thread.join();
    kafka.flush(10000);

    spdlog::info("Stopped. Total published: {}", g_ticks_published.load());
    return 0;
}
