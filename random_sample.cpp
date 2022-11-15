#include <poplar/Vertex.hpp>

class RandomSampleVertex : public poplar::MultiVertex {
    public:
    // Fields
    poplar::Input<poplar::Vector<int>> local_list;
    poplar::Input<int> over_sampling_factor;
    poplar::Output<poplar::Vector<int>> sampled_list;

    // Compute function
    bool compute(unsigned workerId) {
        for (std::size_t i = workerId * (over_sampling_factor - 1); i < local_list.size(); i += MultiVertex::numWorkers()) {
            sampled_list[i / over_sampling_factor] = local_list[i];
        }
        return true;
    }
};