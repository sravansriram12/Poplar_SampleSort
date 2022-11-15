#include <poplar/Vertex.hpp>
#include <stdlib.h>

class RandomSampleVertex : public poplar::MultiVertex {
    public:
    // Fields
    poplar::Input<poplar::Vector<int>> local_list;
    poplar::Input<int> over_sampling_factor;
    poplar::Output<poplar::Vector<int>> sampled_list;

    // Compute function
    bool compute(unsigned workerId) {
        for (std::size_t i = workerId * over_sampling_factor; i < local_list.size(); i += MultiVertex::numWorkers()) {
            sampled_list.push_back(local_list[i]);
        }
    }
};