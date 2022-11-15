#include <iostream>
#include <ArrayRef.hpp>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <stdlib.h>
#include <string>


using namespace poplar;
using namespace poplar::program;
using std::to_string;

int main() {
  // Create the IPU model device

  unsigned n = 30;  // number of elements
  unsigned k = 3;   // oversampling factor
  unsigned p = 3;   // number of processors (tiles)
  unsigned local_list_size = n / p;
  srand (time(NULL));
  
  IPUModel ipuModel;
  Device device = ipuModel.createDevice();
  Target target = device.getTarget();

  // Create the Graph object
  Graph graph(target);

  // Add codelets to the graph
  graph.addCodelets("random_sample.cpp");

  // Create a control program that is a sequence of steps
  Sequence prog;
  Tensor list = graph.addConstant<float>(FLOAT, {n}, 0);
  Tensor full_sampled = graph.addVariable(FLOAT, {p * k}, "full_sampled");
  graph.setTileMapping(list, p + 1);
  graph.setTileMapping(full_sampled, p + 1);
  for (unsigned idx = 0; idx < n; idx++) {
        list[idx] = rand();
  }
  ComputeSet computeSet = graph.addComputeSet("computeSet");
 
  for (unsigned i = 0; i < p; i++) {
    Tensor variableI = graph.addVariable(FLOAT, {local_list_size}, "v" + to_string(i));
    graph.setTileMapping(variableI, i);
    prog.add(Copy(list.slice(local_list_size * i, local_list_size * (i + 1)), variableI));
    VertexRef vtx = graph.addVertex(computeSet, "RandomSampleVertex");
    graph.connect(vtx["local_list"], variableI);
    graph.connect(vtx["out"], full_sampled.slice(i * k, (i + 1) * k));
    graph.setTileMapping(vtx, i);
  }


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