#include <poplar/Vertex.hpp>
using namespace poplar;

/*
class RandomSampleVertex : public MultiVertex {
    public:
    // Fields
    Input<Vector<int>> local_list;
    Input<int> over_sampling_factor;
    Output<Vector<int>> sampled_list;

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
}; */

class QuickSort : public Vertex {
    public:
    // Fields
    InOut<Vector<int>> local_list;
    Input<int> num_processors;

    int partition(int low, int high) {
        int pivot = local_list[high]; // pivot
        int i = (low - 1);
    
        for (int j = low; j <= high - 1; j++) {
            if (local_list[j] < pivot) {
                i++;
                int temp = local_list[i];
                local_list[i] = local_list[j];
                local_list[j] = temp;
            }
        }
        int temp = local_list[i + 1];
        local_list[i + 1] = local_list[high];
        local_list[high] = temp;
        return (i + 1);
    }
  
    void quickSort(int low, int high) {
        if (low < high) {
            int pi = partition(low, high);
            quickSort(low, pi - 1);
            quickSort(pi + 1, high);
        }
    }

    bool compute() {
      quickSort(0, local_list.size() - 1);
      return true;
    }
};

class LocalSamples : public MultiVertex {
    public: 
    // Fields
    Input<Vector<int>> local_sorted_list;
    Input<int> num_processors;
    Output<Vector<int>> local_samples;

    bool compute(unsigned workerId) {
      unsigned starting_position = (local_sorted_list.size() / num_processors) * (workerId + 1) - 1;
      unsigned increment_by = MultiVertex::numWorkers() * (local_sorted_list.size() / num_processors) + 1;
      for (int i = starting_position; i < local_sorted_list.size(); i += increment_by) {
        unsigned output_index = (starting_position / (local_sorted_list.size() / num_processors)) - 1;
        local_samples[output_index] = local_sorted_list[i];
      }
      return true;
    }
};
