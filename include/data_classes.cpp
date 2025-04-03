#include "data_classes.h"
#include <functional>
#include <string>

// Compute a partition index using a hash of crash_date and crash_time.
// For example, if totalNodes is 5, this function returns a value between 0 and 4.
int getPartition(const CollisionRecord& record, int totalNodes) {
    std::string key = std::to_string(record.crash_date) + std::to_string(record.crash_time);
    std::hash<std::string> hash_fn;
    size_t hash = hash_fn(key);
    return static_cast<int>(hash % totalNodes);
}
