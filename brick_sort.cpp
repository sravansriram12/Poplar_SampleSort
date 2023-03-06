#include <iostream>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <poplar/DeviceManager.hpp>
#include <popops/TopK.hpp>
#include <popops/SortOrder.hpp>
#include <popops/codelets.hpp>
#include <poplar/DebugContext.hpp>
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

int main(int argc, char *argv[]) {
  // Create the IPU model device
  if (argc != 2) {
    cout << "Error in number of arguments" << endl;
  }

  unsigned n = atoi(argv[argc - 1]);  // number of elements
  const char *dev = "model-ipu2";
  srand(time(NULL));
  
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


  srand48(0);
  
  auto input_list = std::vector<int>(n);
  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = (int) mrand48();
  }

  struct timespec start, stop;
  double total_time;
  clock_gettime(CLOCK_REALTIME, &start);

  Graph graph(device);
  Sequence prog;

  // Add codelets to the graph
  graph.addCodelets("vertices.cpp");
  Tensor initial_list = graph.addVariable(INT, {n}, "initial_list");
  int active_numbers_even = n % 2 == 0 ? n : n - 1;
  float div_even = float(active_numbers_even / 2) / 1472;
  int even_pairs_per_tile = ceil(div_even);
  Tensor evenTensor = graph.addVariable(INT, {active_numbers_even}, "even_list");
  
  int active_numbers_odd = n % 2 == 0 ? n - 2 : n - 1;
  float div_odd = float(active_numbers_odd / 2) / 1472;
  Tensor oddTensor = graph.addVariable(INT, {active_numbers_odd}, "odd_list");
  int odd_pairs_per_tile = ceil(div_odd);

  graph.setTileMapping(initial_list, 0);

  int tile_num = 0;
  
  for (int k = 0; k < n; k++) {
    ComputeSet evenset = graph.addComputeSet("Even bubble" + to_string(k));

    tile_num = 0;
    for(int i = 0; i < active_numbers_even; i += (even_pairs_per_tile * 2)) {
      VertexRef brickSort_vtx = graph.addVertex(evenset, "BrickSortComparison");
      graph.setTileMapping(brickSort_vtx, tile_num);
      int end_index = std::min(active_numbers_even, i + (even_pairs_per_tile * 2));
      graph.connect(brickSort_vtx["subtensor"], initial_list.slice(i, end_index));
      graph.setPerfEstimate(brickSort_vtx, 20);
      tile_num++;
    }
    prog.add(Execute(evenset));
    ComputeSet oddset = graph.addComputeSet("Odd bubble" + to_string(k));
    
    tile_num = 0;
    for(int i = 1; i < active_numbers_odd; i += (odd_pairs_per_tile * 2)) {
      VertexRef brickSort_vtx = graph.addVertex(oddset, "BrickSortComparison");
      graph.setTileMapping(brickSort_vtx, tile_num);
      int end_index = std::min(active_numbers_odd, i + (odd_pairs_per_tile * 2));
      graph.connect(brickSort_vtx["subtensor"], initial_list.slice(i, end_index));
      graph.setPerfEstimate(brickSort_vtx, 20);
      tile_num++;
    }
    prog.add(Execute(oddset));
  }

  graph.createHostWrite("list-write", initial_list);
  
  Engine engine(graph, prog, OptionFlags{{"debug.retainDebugInformation", "true"}});
  engine.load(device);
  engine.writeTensor("list-write", input_list.data(), input_list.data() + input_list.size());

  engine.run(0);
  
  engine.printProfileSummary(cout, {{"showExecutionSteps", "true"}});
  

  clock_gettime(CLOCK_REALTIME, &stop);
  total_time = (stop.tv_sec-start.tv_sec)
  +0.000000001*(stop.tv_nsec-start.tv_nsec);

  cout << "Total time (s): " << total_time << endl;

  return 0;
}