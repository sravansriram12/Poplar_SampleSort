#include <vector>
#include <algorithm>
#include <iostream>
using std::cout, std::endl;
using std::to_string;

int main(int argc, char *argv[]) {
  // Create the IPU model device
  if (argc != 2) {
    cout << "Error in number of arguments" << endl;
  }

  unsigned n = atoi(argv[argc - 1]);  // number of elements

  srand48(0);
  
  auto input_list = std::vector<int>(n);
  for (unsigned idx = 0; idx < n; ++idx) {
    input_list[idx] = (int) mrand48();
  }

  struct timespec start, stop;
  double total_time;
  clock_gettime(CLOCK_REALTIME, &start);

  std::sort(input_list.begin(), input_list.end());

  clock_gettime(CLOCK_REALTIME, &stop);
  total_time = (stop.tv_sec-start.tv_sec)
  +0.000000001*(stop.tv_nsec-start.tv_nsec);

  cout << "Total time (s): " << total_time << endl;

  return 0;
}