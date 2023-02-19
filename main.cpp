#include <iostream>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <poplar/DeviceManager.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace poplar;
using namespace poplar::program;
using std::cout, std::endl;
using std::to_string;

void heap_sort(ComputeSet& computeSet, Graph& graph, Tensor input_list, unsigned processorId) {
    VertexRef heapSort_vtx = graph.addVertex(computeSet, "HeapSort");
    graph.connect(heapSort_vtx["local_list"], input_list);
    graph.setTileMapping(heapSort_vtx, processorId);
    graph.setPerfEstimate(heapSort_vtx, 20);

}

void sampling(ComputeSet& computeSet, Graph& graph, Tensor input_list, Tensor output_list, unsigned factor, unsigned processorId) {
    VertexRef sample_vtx = graph.addVertex(computeSet, "Samples");
    graph.connect(sample_vtx["local_sorted_list"], input_list);
    graph.connect(sample_vtx["factor"], factor);
    graph.connect(sample_vtx["local_samples"], output_list);
    graph.setTileMapping(sample_vtx, processorId);
    graph.setPerfEstimate(sample_vtx, 20);
}

void find_processor(ComputeSet& computeSet, Graph& graph, Tensor input_list, Tensor global_samples, unsigned processorId) {
    VertexRef processor_vtx = graph.addVertex(computeSet, "DetermineProcessor");
    graph.connect(processor_vtx["local_list"], input_list);
    graph.connect(processor_vtx["global_samples"], global_samples);
    graph.setTileMapping(processor_vtx, processorId);
    graph.setPerfEstimate(processor_vtx, 20);
}

void print_host_list(std::vector<int> list) {
  cout << "[";
  
  for (unsigned idx = 0; idx < list.size(); ++idx) {
      cout << list[idx];
      if (idx < list.size() - 1) {
        cout << ", ";
      }
  }

  cout << "]" << endl;
}


int main(int argc, char *argv[]) {
  // Create the IPU model device
  if (argc != 5) {
    cout << "Error in number of arguments" << endl;
    return 0;
  }
  
  unsigned n = atoi(argv[argc - 4]);  // number of elements
  unsigned p = atoi(argv[argc - 3]);   // number of processors (tiles)
  if (p < 2 || n % p != 0) {
    cout << "Error in number of processors; number of processors must be greater than 1 and divisible by size of list" << endl;
    return 0;
  }
  unsigned k = atoi(argv[argc - 2]);
  if ((k * p) < p - 1 || k >= n/p) {
    cout << "Error in oversampling factor; (factor * number of processors) be atleast equal to one less than the number of processors and factor must be lesser than local list size of each processor" << endl;
    return 0;
  }
  unsigned local_list_size = n / p;
  unsigned DEBUG = atoi(argv[argc - 1]);
  const char *dev = "ipu";
  
  Device device;

  if (strcmp(dev, "ipu") == 0) {
    // The DeviceManager is used to discover IPU devices
    auto manager = DeviceManager::createDeviceManager();

    // Attempt to attach to a single IPU:
    auto devices = manager.getDevices(poplar::TargetType::IPU, 1);
    std::cout << "Trying to attach to IPU\n";
    auto it = std::find_if(devices.begin(), devices.end(),
                           [](Device &device) { return device.attach(); });

    if (it == devices.end()) {
      std::cerr << "Error attaching to device\n";
      return 1; // EXIT_FAILURE
    }

    device = std::move(*it);
    std::cout << "Attached to IPU " << device.getId() << std::endl;
  } else {
    char ipuVersion[] = "ipu1";
    strncpy(ipuVersion, &dev[6], strlen(ipuVersion));
    IPUModel ipuModel(ipuVersion);
    ipuModel.minIPUSyncDelay = 0;
    ipuModel.relativeSyncDelay = IPUModel::RelativeSyncDelayType::NO_DELAY;
    device = ipuModel.createDevice();
  }

  // initial list of data that is copied from host to device
  srand48(0);
  
  auto input_list = std::vector<int>(n);
  auto processor_list = std::vector<unsigned>(n);
  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = (int) mrand48();
  }

  if (DEBUG == 1) {
    if (n <= 500) {
      cout << "Initial unsorted list: " << endl;
      print_host_list(input_list);
      cout << endl;
    }
   
  }
  
  // Create the Graph object
  Graph graph(device);
  Sequence prog;

  // Add codelets to the graph
  graph.addCodelets("vertices.cpp");

  // Determine compute sets
  ComputeSet local_sample = graph.addComputeSet("Local samples");
  ComputeSet sort_compiled_samples = graph.addComputeSet("Sort compiled samples");
  ComputeSet sample_compiled_samples = graph.addComputeSet("Sample compiled samples");
  ComputeSet determine_processors = graph.addComputeSet("Determine processors");
  ComputeSet local_sort = graph.addComputeSet("Local sort");

  // 2D tensor where each inner tensor at index i represents the initial list at processor i
  Tensor initial_list = graph.addVariable(INT, {n}, "initial_list");
  // Compilation of samples from each processor's local list
  Tensor compiled_samples = graph.addVariable(INT, {p * k}, "compiled_samples");
  graph.setTileMapping(compiled_samples, p);

  // First computation phase - local sampling
  for (unsigned processor = 0; processor < p; processor++) {
    graph.setTileMapping(initial_list.slice(processor * local_list_size, (processor + 1) * local_list_size), processor);
    sampling(local_sample, graph, initial_list.slice(processor * local_list_size, (processor + 1) * local_list_size), 
        compiled_samples.slice(processor * k, (processor + 1) * k), k + 1, processor);
  }


  // Second computation phase - sorting the compilation of the local samples and picking global samples
  quick_sort(sort_compiled_samples, graph, compiled_samples, p);
  Tensor global_samples = graph.addVariable(INT, {p - 1}, "global_samples");
  graph.setTileMapping(global_samples, p);
  sampling(sample_compiled_samples, graph, compiled_samples, global_samples, p, p);

  // Third computation phase - finding buckets belonging to different processor based on global samples
  for (unsigned i = 0; i < p; i++) {
    find_processor(determine_processors, graph, initial_list.slice(i * local_list_size, (i + 1) * local_list_size), 
        global_samples, i);
  }
  
  graph.createHostWrite("list-write", initial_list);
  graph.createHostRead("list-read", initial_list);
  
  // Add sequence of compute sets to program
  prog.add(Execute(local_sample));
  prog.add(Execute(sort_compiled_samples));
  prog.add(Execute(sample_compiled_samples));
  prog.add(Execute(determine_processors));
  prog.add(WriteUndef(global_samples));
  prog.add(WriteUndef(compiled_samples));

  // Run graph and associated prog on engine and device a way to communicate host list to device initial list
  Engine engine(graph, prog, OptionFlags{{"debug.retainDebugInformation", "true"}});
  engine.load(device);
  engine.writeTensor("list-write", input_list.data(), input_list.data() + input_list.size());

  engine.run(0);

  engine.readTensor("list-read", processor_list.data(), processor_list.data() + processor_list.size());

  struct timespec start, stop;
  double total_time;
  clock_gettime(CLOCK_REALTIME, &start);

  std::vector<std::vector<unsigned>> indexes (p, std::vector<unsigned> (0, 0));
  for (unsigned i = 0; i < n; i++) {
    graph.setTileMapping(initial_list[i], processor_list[i]);
    indexes[processor_list[i]].push_back(i);
  }

  std::vector<Tensor> all_processor_lists (p);
  for (unsigned i = 0; i < p; i++) {
    if (indexes[i].size() > 0) {
        Tensor final_tensor = concat(initial_list.slices(indexes[i]));
        quick_sort(local_sort, graph, final_tensor, i);
        all_processor_lists[i] = final_tensor;
    }
  } 

  Tensor sorted_tensor = all_processor_lists[0];
  for (unsigned i = 1; i < p; i++) {
    sorted_tensor = concat(sorted_tensor, all_processor_lists[i]);
  }

  graph.createHostRead("sorted-list-read", sorted_tensor);

  Sequence prog2;
  prog2.add(Execute(local_sort));

  clock_gettime(CLOCK_REALTIME, &stop);
  total_time = (stop.tv_sec-start.tv_sec)
  +0.000000001*(stop.tv_nsec-start.tv_nsec);
 
  Engine engine2(graph, prog2,  OptionFlags{{"debug.retainDebugInformation", "true"}});
  engine2.load(device);
  engine2.writeTensor("list-write", input_list.data(), input_list.data() + input_list.size());

  engine2.run(0);  
  engine2.readTensor("sorted-list-read", input_list.data(), input_list.data() + input_list.size());

 
  if (DEBUG == 1) {
    engine.printProfileSummary(cout, {{"showExecutionSteps", "true"}});
    engine2.printProfileSummary(cout, {{"showExecutionSteps", "true"}});
  }

   if (DEBUG == 1) {
    cout << "FINISHED EXECUTING IPU SAMPLE SORT ALGORITHM" << endl;
    cout << endl;
    if (n <= 500) {
      cout << "Final sorted list: " << endl;
      print_host_list(input_list);
    }
  }

   cout << "IPU-Host Interaction Time (s): " << total_time << endl;


  return 0;
}