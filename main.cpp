#include <chrono>
#include <iostream>

#include "Interpreter.h"
using namespace std::chrono;

int main(int argc, char** argv) {
  Interpreter bf_interpreter;

  // Pass filepath arg to be read in as a str
  std::string instruction_str = bf_interpreter.read_bf_file(argv[argc - 1]);

  // Build the final instructions
  auto preprocess_start = high_resolution_clock::now();
  bf_interpreter.preprocess(instruction_str);
  auto preprocess_end = high_resolution_clock::now();

  // Execute
  auto execution_start = high_resolution_clock::now();
  bf_interpreter.execute();
  auto execution_end = high_resolution_clock::now();

  auto preprocess_duration =
      duration_cast<microseconds>(preprocess_end - preprocess_start);
  auto execution_duration =
      duration_cast<microseconds>(execution_end - execution_start);

  std::cout << "\nPreprocess time: " << preprocess_duration.count()
            << " microseconds\n"
            << "Execution time: " << execution_duration.count()
            << " microseconds\n"
            << "Total time: "
            << preprocess_duration.count() + execution_duration.count()
            << " microseconds\n";
}
