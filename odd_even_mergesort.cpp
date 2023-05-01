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
  if (argc != 3) {
    cout << "Error in number of arguments" << endl;
  }

  int n = atoi(argv[argc - 2]);  // number of elements
  int k = atoi(argv[argc - 1]);
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
  
  

  srand48(0);
  auto input_list = std::vector<int>(n);
  auto dup_list = std::vector<int>(n);
  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = (int) mrand48();
    dup_list[idx] = input_list[idx];
  }

  std::sort(dup_list.begin(), dup_list.end());

  struct timespec cpu_start, cpu_stop, compile_start, compile_stop, engine_start, engine_stop, complete_start, complete_stop;
  double cpu_time, compile_time, engine_time, complete_time;
  clock_gettime(CLOCK_REALTIME, &complete_start);

  Graph graph(device);
  Sequence prog;

  // Add codelets to the graph
  graph.addCodelets("vertices.cpp");
  
  clock_gettime(CLOCK_REALTIME, &cpu_start);
  Tensor initial_list = graph.addVariable(INT, {n}, "initial_list");
  int p = 1472;
  int numbers_per_tile = ceil(float(n) / p);
  int p_in_use = ceil(float(n) / numbers_per_tile);

  int tile_num = 0;
  int nums = 0;

    int even_stop = p_in_use;
    int odd_stop = p_in_use - 1;
    if (p_in_use % 2 != 0) {
        even_stop = p_in_use - 1;
        odd_stop = p_in_use;
    } 

    ComputeSet cs = graph.addComputeSet("localsort");
    std::vector<Tensor> paddings(p_in_use);
    for(int i = 0; i < p_in_use; i++) {
        int end_index = std::min(n, nums + numbers_per_tile);
        VertexRef heapsort_vtx = graph.addVertex(cs, "HeapSort");
        graph.setTileMapping(initial_list.slice(nums, end_index), i);
        graph.connect(heapsort_vtx["local_list"], initial_list.slice(nums, end_index));
        graph.setTileMapping(heapsort_vtx, i);
        nums += numbers_per_tile;
        Tensor p = graph.addVariable(INT, {numbers_per_tile * 2}, "padding"+i);
        graph.setTileMapping(p, i);
        paddings[i] = p;
    }
  
    prog.add(Execute(cs));


    /*

    ComputeSet cs_even = graph.addComputeSet("mergeEven");

    nums = 0;
    int nums2 = nums + numbers_per_tile;
    
    for (int i = 0; i < even_stop; i += 2) { 
        int end_index1 = std::min(n, nums + numbers_per_tile);
        int end_index2 = std::min(n, nums2 + numbers_per_tile);

     
        VertexRef mergesort_vtx = graph.addVertex(cs_even, "MergeSort");
        graph.connect(mergesort_vtx["arr1"], initial_list.slice(nums, end_index1));
        graph.connect(mergesort_vtx["arr2"], initial_list.slice(nums2, end_index2));
        graph.connect(mergesort_vtx["arr3"], paddings[i]);
        graph.connect(mergesort_vtx["numbers"], k);
        graph.connect(mergesort_vtx["per_tile"], numbers_per_tile * 2);

        graph.setTileMapping(mergesort_vtx, i);

   

        nums += (numbers_per_tile * 2);
        nums2 += (numbers_per_tile * 2);
       
    }

    ComputeSet cs_odd = graph.addComputeSet("mergeOdd");

    nums = numbers_per_tile;
    nums2 = nums + numbers_per_tile;
    
    for (int i = 1; i < odd_stop; i += 2) { 
        int end_index1 = std::min(n, nums + numbers_per_tile);
        int end_index2 = std::min(n, nums2 + numbers_per_tile);
   
        VertexRef mergesort_vtx = graph.addVertex(cs_odd, "MergeSort");
        graph.connect(mergesort_vtx["arr1"], initial_list.slice(nums, end_index1));
        graph.connect(mergesort_vtx["arr2"], initial_list.slice(nums2, end_index2));
        graph.connect(mergesort_vtx["arr3"], paddings[i]);
        graph.connect(mergesort_vtx["numbers"], k);
        graph.connect(mergesort_vtx["per_tile"], numbers_per_tile * 2);
        graph.setTileMapping(mergesort_vtx, i);

        nums += (numbers_per_tile * 2);
        nums2 += (numbers_per_tile * 2);
    }

    

    
    for (int i = 0; i < p_in_use; i++) {
        prog.add(Execute(cs_even));
        prog.add(Execute(cs_odd));
    }  */



    for (int i = 0; i < ceil(log2(p_in_use)); i++) {
      ComputeSet csbinary = graph.addComputeSet("csbinary");
      for (int j = 0; j < p_in_use; j += pow(2, i) * 2) {
          if (j % int(pow(2, i)) == 0) {
            int end_index1 = std::min(n, nums + k);
            int end_index2 = std::min(n, nums + (numbers_per_tile * int(pow(2, i))) + k);
            print(nums);
            print(end_index1);
            print(nums + (numbers_per_tile * pow(2, i)));
            print(nums + (numbers_per_tile * int(pow(2, i))) + end_index2);
            VertexRef mergesort_vtx = graph.addVertex(csbinary, "MergeSort");
            graph.connect(mergesort_vtx["arr1"], initial_list.slice(nums, nums + end_index1));
            graph.connect(mergesort_vtx["arr2"], initial_list.slice(nums + (numbers_per_tile * pow(2, i)), nums + (numbers_per_tile * int(pow(2, i))) + end_index2));
            graph.connect(mergesort_vtx["arr3"], paddings[i]);
            graph.connect(mergesort_vtx["numbers"], k);
            graph.connect(mergesort_vtx["per_tile"], k * 2);
            graph.setTileMapping(mergesort_vtx, j);

          }

          nums += (numbers_per_tile * 2);
      }
      prog.add(Execute(csbinary));
    }

    clock_gettime(CLOCK_REALTIME, &cpu_stop);

    
  graph.createHostWrite("list-write", initial_list);
  if (k >= numbers_per_tile * 2) {
     graph.createHostRead("list-read", initial_list.slice(0, k));
  } else {
    graph.createHostRead("list-read-1", initial_list.slice(0, k));
  }
  

  clock_gettime(CLOCK_REALTIME, &compile_start);
  Engine engine(graph, prog, OptionFlags{{"debug.retainDebugInformation", "true"}});
  clock_gettime(CLOCK_REALTIME, &compile_stop);

  engine.load(device);
  engine.writeTensor("list-write", input_list.data(), input_list.data() + input_list.size());
  
  clock_gettime(CLOCK_REALTIME, &engine_start);
  engine.run(0);
  clock_gettime(CLOCK_REALTIME, &engine_stop);
  if (k >= numbers_per_tile * 2) {
    engine.readTensor("list-read", input_list.data(), input_list.data() + k);
  } else {
    engine.readTensor("list-read-1", input_list.data(), input_list.data() + k);
  }
  

  clock_gettime(CLOCK_REALTIME, &complete_stop);
  
  
  cpu_time = (cpu_stop.tv_sec-cpu_start.tv_sec)
  +0.000000001*(cpu_stop.tv_nsec-cpu_start.tv_nsec);

  compile_time = (compile_stop.tv_sec-compile_start.tv_sec)
  +0.000000001*(compile_stop.tv_nsec-compile_start.tv_nsec);

  engine_time = (engine_stop.tv_sec-engine_start.tv_sec)
  +0.000000001*(engine_stop.tv_nsec-engine_start.tv_nsec);

  complete_time = (complete_stop.tv_sec-complete_start.tv_sec)
  +0.000000001*(complete_stop.tv_nsec-complete_start.tv_nsec);

  


  engine.printProfileSummary(cout, {{"showExecutionSteps", "true"}});
  cout << "CPU time: " << cpu_time << endl;
  cout << "Compile time: " << compile_time << endl;
  cout << "Engine time: " << engine_time << endl;
  cout << "Complete time: " << complete_time << endl;

  for (int i = 0; i < k; i++) {
    if (dup_list[i] != input_list[i]) {
        cout << "ERROR: NOT SORTED" << endl;
        break;
    }

    
  }

  return 0;
}