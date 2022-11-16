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

class LocalSort : public Vertex {
    public:
    // Fields
    InOut<Vector<int>> local_list;


    /* This function takes last element as pivot, places
    the pivot element at its correct position in sorted
    array, and places all smaller (smaller than pivot)
    to left of pivot and all greater elements to right
    of pivot */
    int partition(int low, int high) {
        int pivot = local_list[high]; // pivot
        int i = (low - 1); // Index of smaller element and indicates
                    // the right position of pivot found so far
    
        for (int j = low; j <= high - 1; j++) {
            // If current element is smaller than the pivot
            if (local_list[j] < pivot) {
                i++; // increment index of smaller element
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
  
    /* The main function that implements QuickSort
    arr[] --> Array to be sorted,
    low --> Starting index,
    high --> Ending index */
    void quickSort(int low, int high) {
        if (low < high) {
            /* pi is partitioning index, arr[p] is now
            at right place */
            int pi = partition(low, high);
    
            // Separately sort elements before
            // partition and after partition
            quickSort(low, pi - 1);
            quickSort(pi + 1, high);
        }
    }

    // Compute function
    void compute() {
      quickSort(0, local_list.size() - 1);
    }
};