#include <poplar/Vertex.hpp>

class RandomSampleVertex : public poplar::MultiVertex {
    public:
    // Fields
    poplar::Input<poplar::Vector<int>> local_list;
    poplar::Input<int> over_sampling_factor;
    poplar::Output<poplar::Vector<int>> sampled_list;

    // Compute function
    bool compute(unsigned workerId) {
        unsigned new_workerId = workerId + 1;
        unsigned increment_by = MultiVertex::numWorkers() + 1 + (MultiVertex::numWorkers() * (over_sampling_factor - 1));
        unsigned starting_position = workerId + ((workerId + 1) * (over_sampling_factor - 1));
        for (std::size_t i = starting_position; i < local_list.size(); i += increment_by) {
            unsigned output_index = ((i + 1) / over_sampling_factor) - 1;
            sampled_list[output_index] = local_list[i];
        }
        return true;
    }
};