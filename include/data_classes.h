#ifndef DATA_CLASSES_H
#define DATA_CLASSES_H

#include <vector>
#include <mutex>
#include <string>
#include <functional>

// Represents an individual collision record.
class CollisionRecord {
public:
    int crash_date;
    int crash_time;
    int persons_injured;
    int persons_killed;
    int pedestrians_injured;
    int pedestrians_killed;
    int cyclists_injured;
    int cyclists_killed;
    int motorists_injured;
    int motorists_killed;
    
    CollisionRecord(int date, int time, int p_inj, int p_kill,
                    int ped_inj, int ped_kill, int cyc_inj, int cyc_kill,
                    int mot_inj, int mot_kill)
      : crash_date(date), crash_time(time), persons_injured(p_inj), persons_killed(p_kill),
        pedestrians_injured(ped_inj), pedestrians_killed(ped_kill),
        cyclists_injured(cyc_inj), cyclists_killed(cyc_kill),
        motorists_injured(mot_inj), motorists_killed(mot_kill) {}
};

// Manages collision records.
class CollisionDataManager {
public:
    std::vector<CollisionRecord> data;
    std::mutex data_mutex;
    
    // Inserts a collision record into the data vector safely.
    void insertRecord(const CollisionRecord& record) {
        std::lock_guard<std::mutex> lock(data_mutex);
        data.push_back(record);
    }
};

// Partitioning function: returns a partition index based on record fields.
int getPartition(const CollisionRecord& record, int totalNodes);

#endif // DATA_CLASSES_H
