#include "kafka_producer.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <stdexcept>

class KafkaTickProducer::DeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    explicit DeliveryReportCb(std::atomic<uint64_t>& delivered,
                              std::atomic<uint64_t>& errors)
        : delivered_(delivered), errors_(errors) {}

    void dr_cb(RdKafka::Message& message) override {
        if (message.err() != RdKafka::ERR_NO_ERROR) {
            errors_.fetch_add(1, std::memory_order_relaxed);
            spdlog::error("Kafka delivery failed: {}", message.errstr());
        } else {
            delivered_.fetch_add(1, std::memory_order_relaxed);
        }
    }

private:
    std::atomic<uint64_t>& delivered_;
    std::atomic<uint64_t>& errors_;
};

KafkaTickProducer::KafkaTickProducer(const std::string& brokers,
                                     const std::string& topic)
    : topic_name_(topic)
{
    dr_cb_ = std::make_unique<DeliveryReportCb>(delivered_, errors_);

    std::string errstr;
    auto conf = std::unique_ptr<RdKafka::Conf>(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    conf->set("bootstrap.servers", brokers, errstr);
    conf->set("linger.ms",           "5",    errstr);
    conf->set("batch.num.messages",  "1000", errstr);
    conf->set("compression.type",    "lz4",  errstr);
    conf->set("acks",                "1",    errstr);
    conf->set("queue.buffering.max.kbytes", "65536", errstr);
    conf->set("retries",             "3",    errstr);
    conf->set("retry.backoff.ms",    "100",  errstr);

    if (conf->set("dr_cb", dr_cb_.get(), errstr) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Failed to set delivery callback: " + errstr);
    }

    producer_.reset(RdKafka::Producer::create(conf.get(), errstr));
    if (!producer_) {
        throw std::runtime_error("Failed to create Kafka producer: " + errstr);
    }

    spdlog::info("Kafka producer initialized (broker={})", brokers);
}

KafkaTickProducer::~KafkaTickProducer() {
    if (producer_) {
        producer_->flush(10000);
    }
}

bool KafkaTickProducer::produce(const std::string& key, const std::string& value) {
    auto err = producer_->produce(
        topic_name_,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<char*>(value.data()), value.size(),
        key.data(), key.size(),
        0, nullptr);

    if (err != RdKafka::ERR_NO_ERROR) {
        if (err == RdKafka::ERR__QUEUE_FULL) {
            producer_->poll(100);
            return produce(key, value);
        }
        spdlog::error("Kafka produce error: {}", RdKafka::err2str(err));
        return false;
    }

    producer_->poll(0);
    return true;
}

void KafkaTickProducer::flush(int timeout_ms) {
    spdlog::info("Flushing Kafka producer...");
    producer_->flush(timeout_ms);
    spdlog::info("Kafka flush complete (delivered={}, errors={})",
                 delivered_.load(), errors_.load());
}

bool KafkaTickProducer::wait_for_broker(int timeout_sec) {
    spdlog::info("Waiting for Kafka broker (timeout={}s)...", timeout_sec);

    for (int i = 0; i < timeout_sec; ++i) {
        RdKafka::Metadata* metadata = nullptr;
        auto err = producer_->metadata(true, nullptr, &metadata, 2000);

        if (err == RdKafka::ERR_NO_ERROR && metadata) {
            spdlog::info("Kafka broker ready ({} broker(s))", metadata->brokers()->size());
            delete metadata;
            return true;
        }
        if (metadata) delete metadata;

        spdlog::info("Waiting for Kafka... ({}/{})", i + 1, timeout_sec);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    spdlog::error("Kafka broker not reachable after {}s", timeout_sec);
    return false;
}
