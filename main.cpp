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


using namespace poplar;
using namespace poplar::program;
using namespace popops;
using std::to_string;

void quick_sort(ComputeSet& computeSet, Graph& graph, Tensor local_list, unsigned processorId) {
    VertexRef quickSort_vtx = graph.addVertex(computeSet, "QuickSort");
    graph.connect(quickSort_vtx["local_list"], local_list);
    graph.setTileMapping(quickSort_vtx, processorId);
    graph.setPerfEstimate(quickSort_vtx, 20);

}

void sampling(ComputeSet& computeSet, Graph& graph, Tensor input_list, Tensor output_list, unsigned p, unsigned processorId) {
    VertexRef sample_vtx = graph.addVertex(computeSet, "Samples");
    graph.connect(sample_vtx["local_sorted_list"], input_list);
    graph.connect(sample_vtx["num_processors"], p);
    graph.connect(sample_vtx["local_samples"], output_list);
    graph.setTileMapping(sample_vtx, processorId);
    graph.setPerfEstimate(sample_vtx, 20);
}

void bin_buckets(ComputeSet& computeSet, Graph& graph, Tensor input_list, Tensor global_samples, Tensor output_list, unsigned processorId) {
    VertexRef boundaries_vtx = graph.addVertex(computeSet, "DetermineBuckets");
    graph.connect(boundaries_vtx["local_sorted_list"], input_list);
    graph.connect(boundaries_vtx["global_samples"], global_samples);
    graph.connect(boundaries_vtx["index_boundaries"], output_list);
    graph.setTileMapping(boundaries_vtx, processorId);
    graph.setPerfEstimate(boundaries_vtx, 20);
}

int main() {
  // Create the IPU model device

  unsigned n = 50;  // number of elements
  unsigned p = 5;   // number of processors (tiles)
  unsigned local_list_size = n / p;
  const char *dev = "model-ipu2";
  srand (time(NULL));
  
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

  // Create the Graph object
  Graph graph(device);
  Sequence prog;

  // Add codelets to the graph
  graph.addCodelets("vertices.cpp");
  // Determine compute sets
  ComputeSet local_sort = graph.addComputeSet("Local sort");
  ComputeSet local_sample = graph.addComputeSet("Local samples");
  ComputeSet sort_compiled_samples = graph.addComputeSet("Sort compiled samples");
  ComputeSet sample_compiled_samples = graph.addComputeSet("Sample compiled samples");
  ComputeSet determine_buckets = graph.addComputeSet("Determine buckets");

  // initial list of data that is copied from host to device
  auto input_list = std::vector<int>(n);
  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = rand() % 100;
  }


  // 2D tensor where each inner tensor at index i represents the initial list at processor i
  Tensor initial_list = graph.addVariable(INT, {p, local_list_size}, "initial_list");
  // Compilation of samples from each processor's local list
  Tensor compiled_samples = graph.addVariable(INT, {p * (p - 1)}, "compiled_samples");
  graph.setTileMapping(compiled_samples, p);

  // First computation phase - local sorting and sampling
  for (unsigned processor = 0; processor < p; processor++) {
    graph.setTileMapping(initial_list[processor], processor);
    quick_sort(local_sort, graph, initial_list[processor], processor); 
    sampling(local_sample, graph, initial_list[processor], 
        compiled_samples.slice(processor * (p - 1), (processor + 1) * (p - 1)), p, processor);
  }

   

  // Second computation phase - sorting the compilation of the local samples and picking global samples
  quick_sort(sort_compiled_samples, graph, compiled_samples, p);
  Tensor global_samples = graph.addVariable(INT, {p - 1}, "global_samples");
  graph.setTileMapping(global_samples, p);
  sampling(sample_compiled_samples, graph, compiled_samples, global_samples, p, p);


  // Third computation phase - finding buckets belonging to different processor based on global samples
  //Tensor buckets = graph.addVariable(INT, {p, p - 1}, "buckets");
  //auto bucket_list = std::vector<int>(p * (p - 1)); 

  for (unsigned processor = 0; processor < p; processor++) {
    //graph.setTileMapping(buckets[processor], processor);
    int index_boundary = 0;
    graph.setTileMapping(index_boundary, processor);
    //bin_buckets(determine_buckets, graph, initial_list[processor], global_samples, buckets[processor], processor);
    VertexRef boundaries_vtx = graph.addVertex(determine_buckets, "DetermineBuckets");
    graph.connect(boundaries_vtx["local_sorted_list"], initial_list[processor]);
    graph.connect(boundaries_vtx["global_samples"], global_samples[0]);
    graph.connect(boundaries_vtx["index_boundaries"], index_boundary);
    graph.setTileMapping(boundaries_vtx, processor);
    graph.setPerfEstimate(boundaries_vtx, 20);
  }

  auto in_stream_list = graph.addHostToDeviceFIFO("initial_list", INT, n);
  //auto bucket_stream_list = graph.addDeviceToHostFIFO("bucket_list", INT, p * (p - 1));
  
  // Add sequence of compute sets to program
  prog.add(Copy(in_stream_list, initial_list));
  prog.add(PrintTensor("initial lists", initial_list));
  prog.add(Execute(local_sort));
  prog.add(PrintTensor("locally sorted lists", initial_list));
  prog.add(Execute(local_sample));
  prog.add(PrintTensor("initially compiled samples", compiled_samples));
  prog.add(Execute(sort_compiled_samples));
  prog.add(PrintTensor("sorted compiled samples", compiled_samples));
  prog.add(Execute(sample_compiled_samples));
  prog.add(PrintTensor("global samples", global_samples));
  prog.add(Execute(determine_buckets));
  //prog.add(PrintTensor("bucket boundaries of each processor", buckets));
  //prog.add(Copy(buckets, bucket_stream_list));
  //prog.add(PrintTensor("a", a));

  // Run graph and associated prog on engine and device a way to communicate host list to device initial list
  Engine engine(graph, prog);
  engine.load(device);
  engine.connectStream("initial_list", input_list.data());
  //engine.connectStream("bucket_list", bucket_list.data());

  // Run the control program
  engine.run(0);

  return 0;
}