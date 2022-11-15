#include <poplar/Vertex.hpp>

class RandomSampleVertex : public poplar::MultiVertex {
    public:
    // Fields
    poplar::Input<poplar::Vector<int>> local_list;
    poplar::Output<poplar::Vector<int>> sampled_list;

    // Compute function
    bool compute(unsigned workerId) {
        for (std::size_t i = workerId; i < sampled_list.size(); i += MultiVertex::numWorkers()) {
            sampled_list[i] = 2;
        }
        return true;
    }
};