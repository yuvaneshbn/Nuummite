#pragma once
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include "opus_codec.h"
#include "jitter_buffer.h"

#if defined(_MSC_VER)
#include <immintrin.h>
#endif

struct AudioStreamState {
    std::string id;
    std::unique_ptr<OpusCodec> decoder;
    JitterBuffer jitter_buffer;
    std::atomic_flag lock = ATOMIC_FLAG_INIT;
    std::atomic<float> peak_volume{0.0f};

    bool tryAcquireLock() noexcept {
        return !lock.test_and_set(std::memory_order_acquire);
    }
    void acquireLock() noexcept {
        while (lock.test_and_set(std::memory_order_acquire)) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
            _mm_pause();
#endif
        }
    }
    void releaseLock() noexcept {
        lock.clear(std::memory_order_release);
    }
};

struct ActiveStreamSnapshot {
    std::vector<AudioStreamState*> streams;
};

class LockFreeStreamDirectory {
public:
    LockFreeStreamDirectory() {
        std::atomic_store(&snapshot_, std::make_shared<ActiveStreamSnapshot>());
    }

    std::shared_ptr<const ActiveStreamSnapshot> acquire_snapshot() noexcept {
        return std::atomic_load(&snapshot_);
    }

    void register_stream(const std::string& client_id, std::unique_ptr<AudioStreamState> new_stream) {
        (void)client_id;
        std::lock_guard<std::mutex> lock(directory_mutex_);
        managed_streams_.push_back(std::move(new_stream));

        auto next_snapshot = std::make_shared<ActiveStreamSnapshot>();
        next_snapshot->streams.reserve(managed_streams_.size());
        for (const auto& item : managed_streams_) {
            next_snapshot->streams.push_back(item.get());
        }

        std::atomic_store(&snapshot_, std::move(next_snapshot));
    }

    void unregister_all() {
        std::lock_guard<std::mutex> lock(directory_mutex_);
        managed_streams_.clear();
        std::atomic_store(&snapshot_, std::make_shared<ActiveStreamSnapshot>());
    }

private:
    std::mutex directory_mutex_;
    std::vector<std::unique_ptr<AudioStreamState>> managed_streams_;
    std::shared_ptr<ActiveStreamSnapshot> snapshot_;
};
