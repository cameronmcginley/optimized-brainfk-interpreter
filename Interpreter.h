#pragma once
#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <string>
#include <vector>

class Interpreter {
  // The "tape" of bytes
  std::vector<char> tape;
  int curr_tape_index;

  std::string execution_str;
  std::vector<int> execution_data;

  std::string execution_string2 = "";
  std::vector<int> execution_data2;

  int curr_instruction_index;

  bool debug = false;

  std::string debug_store_output = "";

 public:
  Interpreter();

  // Log overloaded for different scenarios
  // String
  void log(std::string log_str);
  // All instruction data
  void log(int instr_num, char instr, int instr_data, int cell_num,
           int cell_data);
  // Vector
  void log(std::vector<int> log_vec);
  // Instruction string with data vector
  void log(std::string log_str, std::vector<int> log_vec);

  // Takes in a filepath, returns it as a string
  std::string read_bf_file(std::string filename);

  // Begins the preprocessing for the raw instruction string
  // Stores the final instruction feed and respective data bytes
  // in execution_string2 and execution_data2 respectively
  void preprocess(std::string raw_instructions);

  // Handled by preprocess()
  // Throws error if bad syntax (wrong char set, non closing loops)
  void syntax_check(std::string raw_instructions);

  // Handled by preprocess()
  // Iterates through the string of instructions, detects duplicates
  // removes them, and adds the count-1 to a parallel vector
  void coalesce_operands(std::string instructions);

  // Handled by pattern_match()
  // Simply checks whether "<" and ">" are balanced in a str
  // Returns the num occurences if so, else returns 0
  int check_balance_traverse(std::string str);

  // Handled by coalesce_operands()
  // Takes in (uncondensed) instruction string, regexes various patterns
  // and replaces them with a new instruction key, returns modified string
  std::string pattern_match(std::string instructions);

  // Handled by preprocess()
  // Runs on the final condensed instruction str, matches loop brackets
  // Stores index+1 of a bracket's pair, in the parallel data vector
  void matching_loop();

  // Interprets the final string of instructions
  void execute();
};

#endif