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

  int n = atoi(argv[argc - 1]);  // number of elements
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
  int p = 1000;
  int numbers_per_tile = ceil(float(n) / p);
  int p_in_use = n / numbers_per_tile;

  int tile_num = 0;
  int nums = 0;
  for(int i = 0; i < p_in_use; i++) {
    int end_index = std::min(n, nums + numbers_per_tile);
    graph.setTileMapping(initial_list.slice(nums, end_index), i);
    nums += numbers_per_tile;
  }

  for (int k = 0; k < ceil(log2(n)); k++) {
      ComputeSet cs_even = graph.addComputeSet("mergeEven"+to_string(k));
      
      int nums = 0;
      int nums2 = nums + numbers_per_tile;
      for (int i = 0; i < p_in_use; i += 2) {
        int end_index1 = std::min(n, nums + numbers_per_tile);
        int end_index2 = std::min(n, nums2 + numbers_per_tile);
        VertexRef heapsort_vtx = graph.addVertex(cs_even, "HeapSort");
        graph.connect(heapsort_vtx["local_list"], concat(initial_list.slice(nums, end_index1), initial_list.slice(nums2, end_index2)));
        graph.setTileMapping(heapsort_vtx, i);
        graph.setPerfEstimate(heapsort_vtx, 20);
        nums += (numbers_per_tile * 2);
        nums2 += (numbers_per_tile * 2);
      }
      prog.add(Execute(cs_even));

      ComputeSet cs_odd = graph.addComputeSet("mergeOdd"+to_string(k));
       
      nums = numbers_per_tile;
      nums2 = nums + numbers_per_tile;
      for (int i = 1; i < p_in_use - 1; i += 2) {
        int end_index1 = std::min(n, nums + numbers_per_tile);
        int end_index2 = std::min(n, nums2 + numbers_per_tile);
        VertexRef heapsort_vtx = graph.addVertex(cs_odd, "HeapSort");
        graph.connect(heapsort_vtx["local_list"], concat(initial_list.slice(nums, end_index1), initial_list.slice(nums2, end_index2)));
        graph.setTileMapping(heapsort_vtx, i);
        graph.setPerfEstimate(heapsort_vtx, 20);
        nums += (numbers_per_tile * 2);
        nums2 += (numbers_per_tile * 2);
      }

      prog.add(Execute(cs_odd));
  }

  prog.add(PrintTensor(initial_list));
  
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