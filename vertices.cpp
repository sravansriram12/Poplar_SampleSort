#include <poplar/Vertex.hpp>
using namespace poplar;

class QuickSort : public Vertex {
    public:
    // Fields
    InOut<Vector<int>> local_list;

    void exchange(int A, int B) {
        int temp = local_list[A];
        local_list[A] = local_list[B];
        local_list[B] = temp;
    }

    int median_three(int low, int high) {
       unsigned mid = low + (high - low) / 2;
       if (a[high] < a[low]) exchange(low, high);
       if (a[mid] < a[low]) exchange(low, mid);
       if (a[high] < a[mid]) exchange(mid, high);
       return mid;
    }

    int partition(int low, int high) {
        unsigned i = low, j = high + 1;
        int pivot = local_list[low];
        while(1) {
            while (a[++i] < pivot) {
                if (i == high) break;
            }
            while (pivot < a[--j]) {
                if (j == low) break;
            }
            if (i >= j) break;
            exchange(i, j);
        }
        exchange(low, j);
        return j;
    }
  
    void quickSort(int low, int high) {
        if (low < high) {
            int median = median_three(local_list[low], local_list[(low + high) / 2], local_list[high]);
            exchange(low, median);
            unsigned pi = partition(low, high);
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