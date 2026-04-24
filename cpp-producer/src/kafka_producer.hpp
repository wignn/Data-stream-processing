#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <librdkafka/rdkafkacpp.h>

class KafkaTickProducer {
public:
    KafkaTickProducer(const std::string& brokers, const std::string& topic);
    ~KafkaTickProducer();

    KafkaTickProducer(const KafkaTickProducer&) = delete;
    KafkaTickProducer& operator=(const KafkaTickProducer&) = delete;

    bool produce(const std::string& key, const std::string& value);
    void flush(int timeout_ms = 5000);
    bool wait_for_broker(int timeout_sec = 60);

    uint64_t delivered_count() const { return delivered_.load(std::memory_order_relaxed); }
    uint64_t error_count() const { return errors_.load(std::memory_order_relaxed); }

private:
    class DeliveryReportCb;

    std::unique_ptr<RdKafka::Producer> producer_;
    std::unique_ptr<DeliveryReportCb>  dr_cb_;
    std::string                        topic_name_;
    std::atomic<uint64_t>              delivered_{0};
    std::atomic<uint64_t>              errors_{0};
};
