#pragma once
#include <vector>
#include <assert.h>

// Keep count of the frequeny offset indices
class Histogram 
{
private:
    const int max_counts;
    const int total_indices;
    std::vector<int> index_counts;
    std::vector<int> index_queue;
    int total_counts;
    int curr_count_index;
public:
    Histogram(const int _total_indices, const int _max_counts=100)
    : max_counts(_max_counts),
      total_indices(_total_indices)
    {
        index_queue.resize(max_counts, 0);
        index_counts.resize(total_indices, 0);
        total_counts = 0;
        curr_count_index = 0;
    }

    void PushIndex(const int index) {
        assert((index >= 0) && (index < total_indices));
        index_queue[curr_count_index] = index;
        index_counts[index]++;

        curr_count_index = (curr_count_index+1) % max_counts;
        total_counts++;
        if (total_counts > max_counts) {
            total_counts = max_counts;
            const int pop_index = index_queue[curr_count_index];
            index_counts[pop_index]--;
        }
    }

    int GetMode() const {
        int max_count = 0;
        int max_index = 0;

        for (int i = 0; i < total_indices; i++) {
            const int count = index_counts[i];
            if (count > max_count) {
                max_count = count;
                max_index = i;
            }
        }

        return max_index;
    }
};