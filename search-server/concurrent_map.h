#pragma once
#include <type_traits>
#include <map>
#include <mutex>
#include <vector>
#include <execution>


template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

    struct Access {
        Access(Key key, Bucket& bucket) :
            guard(bucket.mutex), ref_to_value(bucket.map[key])
        {
        }
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) :
        size_(bucket_count), buckets_(bucket_count)
    {
    }

    Access operator[](const Key& key) {
        return { key, buckets_[static_cast<uint64_t>(key) % size_] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> ordinary_map;
        for (auto& bucket : buckets_) {
            std::lock_guard<std::mutex> guard(bucket.mutex);
            ordinary_map.insert(bucket.map.begin(), bucket.map.end());
        }
        return ordinary_map;
    }

    void erase(const Key& key) {
        std::lock_guard<std::mutex> guard(buckets_[static_cast<uint64_t>(key) % size_].mutex);
        buckets_[static_cast<uint64_t>(key) % size_].map.erase(key);
        return;
    }

private:
    uint64_t size_;
    std::vector<Bucket> buckets_;
};