#include <poplar/Vertex.hpp>
using namespace poplar;

class QuickSort : public Vertex {
    public:
    // Fields
    InOut<Vector<int>> local_list;

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
      unsigned starting_position = (local_sorted_list.size() / num_processors) * (workerId + 1);
      unsigned increment_by = MultiVertex::numWorkers() * (local_sorted_list.size() / num_processors);
      for (unsigned i = starting_position; i < local_sorted_list.size(); i += increment_by) {
        unsigned output_index = (starting_position / (local_sorted_list.size() / num_processors)) - 1;
        local_samples[output_index] = local_sorted_list[i];
      }
      return true;
    }
};