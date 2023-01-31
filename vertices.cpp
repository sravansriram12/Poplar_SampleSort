#include <poplar/Vertex.hpp>
using namespace poplar;

/*class QuickSort : public Vertex {
    public:
    // Fields
    InOut<Vector<int>> local_list;

    void swap(int *a, int *b) {
        int t = *a;
        *a = *b;
        *b = t;
    }

    int median_three(unsigned low, unsigned high) {
        int pivot;
        int mid = (low + high) / 2;
        if (local_list[mid] < local_list[low]) 
            swap(&local_list[mid], &local_list[low]);
        if (local_list[high] < local_list[low])
            swap(&local_list[high], &local_list[low]);
        if (local_list[high] < local_list[mid])
            swap(&local_list[high], &local_list[mid]);
        swap(&local_list[mid], &local_list[high-1]);
        
        pivot = local_list[high-1];
    
        return partition(low, high);
    }

    int partition(unsigned low, unsigned high) {
        int pivot = local_list[high];
        int i = (low - 1);
    
        for (int j = low; j < high; j++) {
            if (local_list[j] < pivot) {
                swap(&local_list[++i], &local_list[j]);
            }
        }
    
        swap(&local_list[i + 1], &local_list[high]);
        return (i + 1);
    }
  
    void quickSort(unsigned low, unsigned high) {
        if (low < high) {
            int pi = median_three(low, high);
    
            quickSort(low, pi - 1);

            quickSort(pi + 1, high);
	    }
    }

    bool compute() {
      quickSort(0, local_list.size() - 1);
      return true;
    }
}; */

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

class Samples : public MultiVertex {
    public: 
    // Fields
    Input<Vector<int>> local_sorted_list;
    Input<int> factor;
    Output<Vector<int>> local_samples;

    bool compute(unsigned workerId) {
      unsigned starting_position = (local_sorted_list.size() / factor) * (workerId + 1);
      unsigned increment_by = MultiVertex::numWorkers() * (local_sorted_list.size() / factor);
      for (unsigned i = starting_position; i < local_sorted_list.size(); i += increment_by) {
        unsigned output_index = (i / (local_sorted_list.size() / factor)) - 1;
        if (output_index >= local_samples.size()) {
            break;
        }
        local_samples[output_index] = local_sorted_list[i];
      }
      return true;
    }
}; 

class DetermineProcessor: public MultiVertex {
    public:
    InOut<Vector<int>> local_list;
    Input<Vector<int>> global_samples;

     bool compute(unsigned workerId) {
        
        for (unsigned i = workerId; i < local_list.size(); i += MultiVertex::numWorkers()) {
            int end = global_samples.size();
            int target = local_list[i];
            int start = 0;
           
            if (target > global_samples[end - 1]) {
                local_list[i] = end;
                continue;
            }
        
            int ans = -1;
            while (start <= end) {
                int mid = (start + end) / 2;
        
                if (global_samples[mid] >= target) {
                    end = mid - 1;
                } else {
                    ans = mid;
                    start = mid + 1;
                }
            }
            local_list[i] = ans + 1;
        } 
       
        return true;
    }
};