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

class Samples : public MultiVertex {
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

class DetermineProcessor: public Vertex {
    public:
    Input<Vector<int>> local_list;
    Input<Vector<int>> global_samples;
    Output<Vector<int>> processor;

     bool compute() {
        
        for (unsigned i = 0; i < local_list.size(); i++) {
            int end = global_samples.size();
            int target = local_list[i];
            int start = 0;
           
            if (target > global_samples[end - 1]) {
                processor[i] = end;
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
            processor[i] = ans + 1;
        } 
       
        return true;
    }
};

class DetermineBuckets : public Vertex {
    public: 
    // Fields
    Input<Vector<int>> local_sorted_list;
    Input<Vector<int>> global_samples;
    Output<Vector<int>> index_boundaries;

    bool compute() {
        
        for (unsigned i = 0; i < global_samples.size(); i++) {
            int end = local_sorted_list.size();
            int target = global_samples[i];
            int start = 0;
           
            if (target > local_sorted_list[end - 1]) {
                index_boundaries[i] = end-1;
            }
        
            int ans = -1;
            while (start <= end) {
                int mid = (start + end) / 2;
        
                if (local_sorted_list[mid] > target) {
                    end = mid - 1;
                } else {
                    ans = mid;
                    start = mid + 1;
                }
            }
            index_boundaries[i] = ans;
        } 
       
        return true;
    }
};

/*
class MergeLists : public MultiVertex {
    public:
    InOut<Vector<int>> sorted_sub_lists;
    Input<Vector<int>> indexes;
    Output<Vector<int>> sorted_final_list;

     int binary_search_lt(int target, unsigned start, unsigned end) {
        unsigned left = start; 
        unsigned right = end; 

        if (sorted_sub_lists[left] >= target) return left; 
        if (sorted_sub_lists[right] < target) return right+1;
        int mid = (left+right)/2; 
        while (mid > left) {
            if (sorted_sub_lists[mid] < target) {
                left = mid; 
            } else {
                right = mid;
            }
            mid = (left+right)/2;
        }
        return right;
    }

    int binary_search_le(int target, unsigned start, unsigned end) {
    
        unsigned left = start; 
        unsigned right = end; 

        if (sorted_sub_lists[left] > target) return left; 
        if (sorted_sub_lists[right] <= target) return right+1;
        int mid = (left+right)/2; 
        while (mid > left) {
            if (sorted_sub_lists[mid] <= target) {
                left = mid; 
            } else {
                right = mid;
            }
            mid = (left+right)/2;
        }
        return right;
    }
    
    bool compute (unsigned workerId) {
        for (unsigned sublist = workerId; sublist < indexes.size(); sublist += MultiVertex::numWorkers) {
            unsigned neighbor_sublist;
            unsigned start;
            unsigned end;
            if (workerId % 2 == 0) {
                neighbor_sublist = workerId + 1;
                if (neighbor_sublist < indexes.size()) {
                    start = indexes[workerId];
                    end = indexes[neighbor_sublist];
                }
            } else {
                neighbor_sublist = workerId - 1;
                start = neighbor_sublist == 0 ? 0 : indexes[neighbor_sublist];
                end = indexes[workerId];
            }
            for (unsigned i = indexes[sublist]; i < indexes[sublist + 1]; i++) {
                unsigned idx;
                if (workerId % 2 == 0) {
                    if (neighbor_sublist < indexes.size()) {
                        idx = binary_search_lt(sorted_sub_lists[i], start, end); 
                        unsigned j = idx - start;
                        unsigned sorted_idx = i + (idx - start);
                        sorted_final_list[sorted_idx] = sorted_sub_lists[i];
                    } else {
                        sorted_final_list[i] = sorted_sub_lists[i];
                    }
                } else {
                    idx = binary_search_le(sorted_sub_lists[i], start, end); 
                    unsigned sorted_idx = idx + (i - indexes[sublist]);
                    sorted_final_list[sorted_idx] = sorted_sub_lists[i];
                }
            }
        }
        
        return true;
    }
}; */