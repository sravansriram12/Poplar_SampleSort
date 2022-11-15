#include <iostream>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <stdlib.h>
#include <string>
#include <vector>


using namespace poplar;
using namespace poplar::program;
using std::to_string;

int main() {
  // Create the IPU model device

  unsigned n = 30;  // number of elements
  unsigned k = 3;   // oversampling factor
  unsigned p = 3;   // number of processors (tiles)
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

  // Add codelets to the graph
  graph.addCodelets("random_sample.cpp");

  // Create a control program that is a sequence of steps
  Sequence prog;

  auto input_list = std::vector<int>(n);

  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = rand();
  }

  // Set up data streams to copy data in and out of graph
  Tensor initial_list = graph.addVariable(INT, {p, local_list_size}, "initial_list");

  /*Tensor full_sampled = graph.addVariable(INT, {p * k}, "full_sampled");
  graph.setTileMapping(initial_list, p + 1);
  graph.setTileMapping(full_sampled, p + 1);
  for (unsigned idx = 0; idx < n; idx++) {
        list[idx] = rand();
  }
  ComputeSet computeSet = graph.addComputeSet("computeSet"); */

  auto in_stream_list = graph.addHostToDeviceFIFO("initial_list", INT, n);

  Engine engine(graph,
                Sequence({Copy(in_stream_list, initial_list)}),
                engineOpts);
  engine.load(device);
  engine.connectStream("initial_list", input_list.data());

  // Add step to execute the compute set
  prog.add(Execute(computeSet));

  // Create the engine
  Engine engine(graph, prog);
  engine.load(device);

  // Run the control program
  std::cout << "Running program\n";
  engine.run(0);
  std::cout << "Program complete\n";

  return 0;
}

/*
VertexRef vtx = graph.addVertex(computeSet, "RandomSampleVertex");
graph.connect(vtx["local_list"], variableI);
graph.connect(vtx["out"], full_sampled.slice(i * k, (i + 1) * k));
graph.setTileMapping(vtx, i); */