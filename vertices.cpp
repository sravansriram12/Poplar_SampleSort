#include <poplar/Vertex.hpp>
using namespace poplar;

class QuickSort : public Vertex {
    public:
    // Fields
    InOut<Vector<int>> local_list;

    void exchange(unsigned A, unsigned B) {
        int temp = local_list[A];
        local_list[A] = local_list[B];
        local_list[B] = temp;
    }

    int median_three(unsigned low, unsigned high) {
        unsigned mid = (left + right) / 2;

        if (local_list[left] > local_list[center])
            exchange(left, mid);

        if (local_list[left] > local_list[right])
            exchange(left, right);

        if (local_list[mid] > local_list[right])
            exchange(mid, right);

        exchange(mid, right - 1);
        return local_list[right - 1];
    }

    int partition(unsigned low, unsigned high, int pivot) {
        unsigned i = low, j = high - 1;

        while(1) {
            while (local_list[++i] < pivot);
            while (local_list[--j] > pivot);
            if (i >= j) {
                break;
            } else {
                exchange(i, j);
            }
        }
        exchange(i, high - 1);
        return i;
    }
  
    void quickSort(unsigned low, unsigned high) {
        if (low < high) {
            int median = median_three(low, high);
            unsigned pi = partition(low, high, median);
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