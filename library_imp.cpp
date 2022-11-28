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

int main(int argc, char *argv[]) {
  // Create the IPU model device
  if (argc != 3) {
    cout << "Error in number of arguments" << endl;
  }

  unsigned n = atoi(argv[argc - 2]);  // number of elements
  unsigned p = atoi(argv[argc - 1]);   // number of processors (tiles)
  unsigned local_list_size = n / p;
  const char *dev = "model-ipu2";
  srand48(0);
  
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

  struct timespec start, start_qstart, stop, stop_qsort;
  double total_time, time_res, total_time_qsort;

  auto input_list = std::vector<int>(n);
  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = (int) lrand48();
  }


  // 2D tensor where each inner tensor at index i represents the initial list at processor i
  Tensor initial_list = graph.addVariable(INT, {p, local_list_size}, "initial_list");

  // First computation phase - local sorting and sampling
  for (unsigned processor = 0; processor < p; processor++) {
    graph.setTileMapping(initial_list[processor], processor);
  }

  struct timespec start, start_qstart, stop, stop_qsort;
  double total_time, time_res, total_time_qsort;
  
  // Create the Graph object
  Graph graph(device);
  Sequence prog;
  popops::TopKParams params(n, false, SortOrder::ASCENDING, false);
  popops::topK(graph, prog, initial_list, params);

  prog.add(PrintTensor(initial_list));

  graph.createHostWrite("list-write", initial_list);

  Engine engine(graph, prog);
  engine.load(device);
  engine.writeTensor("list-write", input_list.data(), input_list.data() + input_list.size());
  clock_gettime(CLOCK_REALTIME, &start);
  engine.run(0);  
  clock_gettime(CLOCK_REALTIME, &stop);
  total_time = (stop.tv_sec-start.tv_sec)
  +0.000000001*(stop.tv_nsec-start.tv_nsec);

  cout << "Total time (s): " << total_time << endl;

  return 0;
}