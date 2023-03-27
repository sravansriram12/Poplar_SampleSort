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

class MergeSortComparison : public Vertex {
    public:
    InOut<Vector<int>> arr1;
    InOut<Vector<int>> arr2;

    void swap(unsigned idx1, unsigned idx2, int num) {
        if (num == 0) {
            int temp = arr1[idx1];
            arr1[idx1] = arr2[idx2];
            arr2[idx2] = temp;
        } else if (num == 1) {
            int temp = arr1[idx1];
            arr1[idx1] = arr1[idx2];
            arr1[idx2] = temp;
        } else {
            int temp = arr2[idx1];
            arr2[idx1] = arr2[idx2];
            arr2[idx2] = temp;
        }
    } 

    int nextGap(int gap) {
        if (gap <= 1)
            return 0;
        return (gap / 2) + (gap % 2);
    }
 
    void merge() {
        int i, j, gap = arr1.size() + arr2.size();
        for (gap = nextGap(gap);
            gap > 0; gap = nextGap(gap))
        {
            // comparing elements in the first array.
            for (i = 0; i + gap < arr1.size(); i++)
                if (arr1[i] > arr1[i + gap])
                    swap(i, i + gap, 1);
    
            // comparing elements in both arrays.
            for (j = gap > arr1.size() ? gap - arr1.size() : 0;
                i < arr1.size() && j < arr2.size();
                i++, j++)
                if (arr1[i] > arr2[j])
                    swap(i, j, 0);
    
            if (j < arr2.size()) {
                // comparing elements in the second array.
                for (j = 0; j + gap < arr2.size(); j++)
                    if (arr2[j] > arr2[j + gap])
                        swap(j, j + gap, 2);
            }
        }
    }


    bool compute() {
        merge();
        return true;
    }
       
};

class MergeSort : public MultiVertex {
    public:
    InOut<Vector<int>> arr1;
    InOut<Vector<int>> arr2;
    unsigned one = 0;
    unsigned two = 0;
    unsigned three = 0;
    unsigned four = 0;
    unsigned five = 0;
    unsigned six = 0;


    int binary_search_b(int v) {
        int left = 0; 
        int right = (arr1.size() / 2) - 1; 

        if (arr1[left] >= v) return left;
        if (arr1[right] < v) return right+1;
        int mid = (left+right)/2; 
        while (mid > left) {
            if (arr1[mid] < v) {
                left = mid; 
            } else {
                right = mid;
            }
            mid = (left+right)/2;
        }
        return right;
    }

    int binary_search_a(int v) {
        int left = arr1.size() / 2; 
        int right = arr1.size() - 1; 

        if (arr1[left] > v) return left; 
        if (arr1[right] <= v) return right+1;
        int mid = (left+right)/2; 
        while (mid > left) {
            if (arr1[mid] <= v) {
                left = mid; 
            } else {
                right = mid;
            }
            mid = (left+right)/2;
        }
        return right;
    }


    bool compute(unsigned workerId) {
        if (workerId < 3) {
            for (unsigned i = workerId; i < arr1.size() / 2; i += MultiVertex::numWorkers() / 2) {
                arr2[binary_search_b(arr1[i])] = arr1[i];
            }
        } else {
            for (unsigned i = (arr1.size() / 2) + (workerId - 3); i < arr1.size(); i += MultiVertex::numWorkers() / 2) {
                arr2[binary_search_a(arr1[i])] = arr1[i];
            }
        }

        if (workerId == 0) {
            one = 1;
        } else if (workerId == 1) {
            two = 1;
        } else if (workerId == 2) {
            three = 1;
        } else if (workerId == 3) {
            four = 1;
        } else if (workerId == 4) {
            five = 1;
        } else {
            six = 1;
        }

        while (!(one == 1 && two == 1 && three == 1 && four == 1 && five == 1 && six == 1)) {
            continue;
        }


        for (unsigned i = workerId; i < arr1.size(); i += MultiVertex::numWorkers()) {
            arr1[i] = arr2[i];
        }
       
        return true;
    }
       
};