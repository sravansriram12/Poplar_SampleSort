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
  if (argc != 4) {
    cout << "Error in number of arguments" << endl;
  }

  unsigned n = atoi(argv[argc - 3]);  // number of elements
  unsigned p = atoi(argv[argc - 2]);   // number of processors (tiles)
  unsigned DEBUG = atoi(argv[argc - 1]);
  unsigned local_list_size = n / p;
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

  auto input_list = std::vector<int>(n);
  srand48(0);
  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = (int) mrand48();
  }

  Graph graph(device);
  popops::addCodelets(graph);
  Sequence prog;

  struct timespec engine_start, engine_stop;
  double subtract_time;
  // 2D tensor where each inner tensor at index i represents the initial list at processor i
  Tensor initial_list = graph.addVariable(INT, {n});

  // First computation phase - local sorting and sampling
  for (unsigned processor = 0; processor < p; processor++) {
    graph.setTileMapping(initial_list.slice(processor * local_list_size, (processor + 1) * local_list_size), processor);
  } 

  if (DEBUG == 1) {
    prog.add(PrintTensor("initial_list", initial_list));
  }
 
 
  TopKParams params(n, false, SortOrder::ASCENDING, false);
  clock_gettime(CLOCK_REALTIME, &engine_start);
  Tensor final_list = popops::topK(graph, prog, initial_list, params);
  clock_gettime(CLOCK_REALTIME, &engine_stop);

  if (DEBUG == 1) {
    prog.add(PrintTensor("sorted list", final_list));
  }
  
  graph.createHostWrite("list-write", initial_list);
  
  
  Engine engine(graph, prog, OptionFlags{{"debug.retainDebugInformation", "true"}});
  
  subtract_time = (engine_stop.tv_sec-engine_start.tv_sec)
  +0.000000001*(engine_stop.tv_nsec-engine_start.tv_nsec);
  engine.load(device);
  
  engine.writeTensor("list-write", input_list.data(), input_list.data() + input_list.size());
  engine.run(0);  

  cout << "Setting up elements on tile time" << subtract_time << endl;

  if (DEBUG == 1) {
    engine.printProfileSummary(cout, {{"showExecutionSteps", "true"}});
  }
  

  return 0;
}