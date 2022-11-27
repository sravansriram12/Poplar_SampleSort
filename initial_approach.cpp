#include <iostream>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <poplar/DeviceManager.hpp>
#include <popops/TopK.hpp>
#include <popops/SortOrder.hpp>
#include <popops/codelets.hpp>
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
using namespace popops;
using std::cout, std::endl;
using std::to_string;

void quick_sort(ComputeSet& computeSet, Graph& graph, Tensor input_list, unsigned processorId) {
    VertexRef quickSort_vtx = graph.addVertex(computeSet, "QuickSort");
    Tensor stack = graph.addVariable(INT, {input_list.numElements()}, "stack" + to_string(processorId));
    graph.setTileMapping(stack, processorId);
    graph.connect(quickSort_vtx["local_list"], input_list);
    graph.connect(quickSort_vtx["stack"], stack);
    graph.setTileMapping(quickSort_vtx, processorId);
    graph.setPerfEstimate(quickSort_vtx, 20);

}

void sampling(ComputeSet& computeSet, Graph& graph, Tensor input_list, Tensor output_list, unsigned factor, unsigned processorId) {
    VertexRef sample_vtx = graph.addVertex(computeSet, "Samples");
    graph.connect(sample_vtx["local_sorted_list"], input_list);
    graph.connect(sample_vtx["factor"], factor);
    graph.connect(sample_vtx["local_samples"], output_list);
    graph.setTileMapping(sample_vtx, processorId);
    graph.setPerfEstimate(sample_vtx, 20);
}

void find_processor(ComputeSet& computeSet, Graph& graph, Tensor input_list, Tensor global_samples, Tensor output_list, unsigned processorId) {
    VertexRef processor_vtx = graph.addVertex(computeSet, "DetermineProcessor");
    graph.connect(processor_vtx["local_list"], input_list);
    graph.connect(processor_vtx["global_samples"], global_samples);
    graph.connect(processor_vtx["processor"], output_list);
    graph.setTileMapping(processor_vtx, processorId);
    graph.setPerfEstimate(processor_vtx, 20);
}


int main() {
  // Create the IPU model device

  unsigned n = 50;  // number of elements
  unsigned p = 5;   // number of processors (tiles)
  unsigned k = 4;
  unsigned local_list_size = n / p;
  const char *dev = "ipu";
  srand (time(NULL));
  
  Device device;

  if (strcmp(dev, "ipu") == 0) {
    // The DeviceManager is used to discover IPU devices
    auto manager = DeviceManager::createDeviceManager();

    // Attempt to attach to a single IPU:
    auto devices = manager.getDevices(poplar::TargetType::IPU, 10);
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

  struct timespec start, start_qstart, stop, stop_qsort;
  double total_time, time_res, total_time_qsort;
  
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

  // initial list of data that is copied from host to device
  auto input_list = std::vector<int>(n);
  auto processor_list = std::vector<unsigned>(n);
  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = rand() % 100000 + 2;
  }


  // 2D tensor where each inner tensor at index i represents the initial list at processor i
  Tensor initial_list = graph.addVariable(INT, {p, local_list_size}, "initial_list");
  // Compilation of samples from each processor's local list
  Tensor compiled_samples = graph.addVariable(INT, {p * k}, "compiled_samples");
  graph.setTileMapping(compiled_samples, p);

  // First computation phase - local sorting and sampling
  for (unsigned processor = 0; processor < p; processor++) {
    graph.setTileMapping(initial_list[processor], processor);
    //quick_sort(local_sort, graph, initial_list[processor], processor); 
    sampling(local_sample, graph, initial_list[processor], 
        compiled_samples.slice(processor * k, (processor + 1) * k), k + 1, processor);
  }


  // Second computation phase - sorting the compilation of the local samples and picking global samples
  quick_sort(sort_compiled_samples, graph, compiled_samples, p);
  Tensor global_samples = graph.addVariable(INT, {p - 1}, "global_samples");
  graph.setTileMapping(global_samples, p);
  sampling(sample_compiled_samples, graph, compiled_samples, global_samples, p, p);


  // Third computation phase - finding buckets belonging to different processor based on global samples
  Tensor processor_mapping = graph.addVariable(INT, {p, local_list_size}, "processor_mapping");
  for (unsigned i = 0; i < p; i++) {
    graph.setTileMapping(processor_mapping, i);
    find_processor(determine_processors, graph, initial_list[i], global_samples, processor_mapping[i], i);
  }
  

  graph.createHostWrite("list-write", initial_list);
  graph.createHostRead("list-read", initial_list);
  graph.createHostRead("processor-mapping-read", processor_mapping);
  
  // Add sequence of compute sets to program
  //prog.add(PrintTensor(initial_list));
  prog.add(Execute(local_sample));
  //prog.add(PrintTensor(compiled_samples));
  prog.add(Execute(sort_compiled_samples));
  //prog.add(PrintTensor(compiled_samples));
  prog.add(Execute(sample_compiled_samples));
  //prog.add(PrintTensor(global_samples));
  prog.add(Execute(determine_processors));
  //prog.add(WriteUndef(global_samples));

  // Run graph and associated prog on engine and device a way to communicate host list to device initial list
  Engine engine(graph, prog);
  engine.load(device);
  engine.writeTensor("list-write", input_list.data(), input_list.data() + input_list.size());

  clock_gettime(CLOCK_REALTIME, &start);
  engine.run(0);

  engine.readTensor("processor-mapping-read", processor_list.data(), processor_list.data() + processor_list.size());

  initial_list = initial_list.flatten();
  std::vector<std::vector<unsigned>> indexes (p, std::vector<unsigned> (0, 0));
  for (unsigned i = 0; i < n; i++) {
    graph.setTileMapping(initial_list[i], processor_list[i]);
    indexes[processor_list[i]].push_back(i);
  }

  std::vector<Tensor> all_processor_lists (p);
  for (unsigned i = 0; i < p; i++) {
    if (indexes[i].size() > 0) {
        std::vector<unsigned> p_index = indexes[i];
        std::vector<Tensor> tensors = initial_list.slices(p_index);
        Tensor final_tensor = concat(tensors);
        quick_sort(local_sort, graph, final_tensor, i);
        all_processor_lists[i] = final_tensor;
    }
  } 
   
  Sequence prog2;
  prog2.add(Execute(local_sort));
  /*for (unsigned i = 0; i < p; i++) {
    if (indexes[i].size() > 0) {
        prog2.add(PrintTensor("[Proc " + to_string(i) + "]", all_processor_lists[i]));
    }
  } */
  Engine engine2(graph, prog2);
  engine2.load(device);
  engine2.writeTensor("list-write", input_list.data(), input_list.data() + input_list.size());
  engine2.run(0);  

  clock_gettime(CLOCK_REALTIME, &stop);
  total_time = (stop.tv_sec-start.tv_sec)
  +0.000000001*(stop.tv_nsec-start.tv_nsec);

  cout << "Total time (s): " << total_time << endl;



  return 0;
}