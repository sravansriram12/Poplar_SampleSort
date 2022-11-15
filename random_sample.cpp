#include <poplar/Vertex.hpp>
#include <stdlib.h>

class RandomSampleVertex : public poplar::MultiVertex {
    public:
    // Fields
    poplar::Input<poplar::Vector<int>> local_list;
    poplar::Output<poplar::Vector<int>> sampled_list;

    // Compute function
    bool compute(unsigned workerId) {
        for (std::size_t i = workerId; i < local_list.size(); i += MultiVertex::numWorkers()) {
            buckets[i] = rand();
        }
    }
};