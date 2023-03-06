#include <poplar/Vertex.hpp>
using namespace poplar;

class HeapSort: public Vertex {
    public:
    // Fields
    InOut<Vector<int>> local_list;

    void swap(unsigned idx1, unsigned idx2) {
        int temp = local_list[idx1];
        local_list[idx1] = local_list[idx2];
        local_list[idx2] = temp;
    }

    void buildMaxHeap() {
        for (int i = 1; i < local_list.size(); i++)
        {
            // if child is bigger than parent
            if (local_list[i] > local_list[(i - 1) / 2])
            {
                int j = i;
        
                // swap child and parent until
                // parent is smaller
                while (local_list[j] > local_list[(j - 1) / 2])
                {
                    swap(j, (j - 1) / 2);
                    j = (j - 1) / 2;
                }
            }
        }
    }

    void heapSort() {
        buildMaxHeap();

        for (int i = local_list.size() - 1; i > 0; i--)
        {
            // swap value of first indexed
            // with last indexed
            swap(0, i);
        
            // maintaining heap property
            // after each swapping
            int j = 0, index;
            
            do
            {
                index = (2 * j + 1);
                
                // if left child is smaller than
                // right child point index variable
                // to right child
                if (local_list[index] < local_list[index + 1] &&
                                    index < (i - 1))
                    index++;
            
                // if parent is smaller than child
                // then swapping parent with child
                // having higher value
                if (local_list[j] < local_list[index] && index < i)
                    swap(j, index);
            
                j = index;
            
            } while (index < i);
        }
    }

    bool compute() {
      heapSort();
      return true;
    }

};

/* class QuickSort : public Vertex {
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
}; */

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

class BrickSortComparison : public Vertex {
    InOut<Vector<int>> subtensor;

    bool compute() {
        for(int i = 0; i < subtensor.size(); i += 2) {
            if (subtensor[i] > subtensor[i + 1]) {
                int temp = subtensor[i + 1];
                subtensor[i + 1] = subtensor[i];
                subtensor[i] = temp;
            }
        }
    }
};