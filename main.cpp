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

int main() {
  // Create the IPU model device

  unsigned n = 20;  // number of elements
  unsigned p = 2;   // number of processors (tiles)
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
  addCodelets(graph);

  // Add codelets to the graph
  graph.addCodelets("random_sample.cpp");
  ComputeSet local_sort = graph.addComputeSet("Local sort");
  ComputeSet local_sample = graph.addComputeSet("Local samples");

  // Create a control program that is a sequence of steps

  auto input_list = std::vector<int>(n);

  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = rand() % 100;
  }

  // Set up data streams to copy data in and out of graph
  Tensor initial_list = graph.addVariable(INT, {p, local_list_size}, "initial_list");
  Tensor compiled_samples = graph.addVariable(INT, {p * (p - 1)}, "compiled_samples");
  graph.setTileMapping(compiled_samples, p);

  for (unsigned processor = 0; processor < p; processor++) {
    graph.setTileMapping(initial_list[processor], processor);

    VertexRef quickSort_vtx = graph.addVertex(local_sort, "QuickSort");
    graph.connect(quickSort_vtx["local_list"], initial_list[processor]);
    graph.setTileMapping(quickSort_vtx, processor);
    graph.setPerfEstimate(quickSort_vtx, 20);

    VertexRef sample_vtx = graph.addVertex(local_sample, "LocalSamples");
    graph.connect(sample_vtx["local_sorted_list"], initial_list[processor]);
    graph.connect(sample_vtx["num_processors"], p);
    graph.connect(sample_vtx["local_samples"], compiled_samples.slice(processor * (p - 1), (processor + 1) * (p - 1)));
    graph.setTileMapping(sample_vtx, processor);
    graph.setPerfEstimate(sample_vtx, 20);
  }

  auto in_stream_list = graph.addHostToDeviceFIFO("initial_list", INT, n);
  
  prog.add(Copy(in_stream_list, initial_list));
  prog.add(PrintTensor("initial lists", initial_list));
  prog.add(Execute(local_sort));
  prog.add(PrintTensor("locally sorted lists", initial_list));
  prog.add(Execute(local_sample));
  prog.add(PrintTensor("compiled samples", compiled_samples));

  Engine engine(graph, prog);
  engine.load(device);
  engine.connectStream("initial_list", input_list.data());

  // Run the control program
  engine.run(0);

  return 0;
}