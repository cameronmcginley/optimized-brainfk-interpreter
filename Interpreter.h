#pragma once
#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <string>
#include <vector>

class Interpreter {
  // The "tape" of bytes
  std::vector<char> tape;
  int curr_tape_index;

  std::string temp_execution_string;
  std::vector<int> temp_execution_data;

  std::string final_execution_string = "";
  std::vector<int> final_execution_data;
  int curr_instruction_index;

  bool debug = false;
  std::string debug_store_output = "";

 public:
  Interpreter();

  // Log overloaded for different scenarios
  /**
   * Print a string.
   *
   * @param log_str String.
   */
  void debug_log(std::string log_str);

  /**
   * Log all data for an instruction.
   *
   * @param instr_num Index of instruction.
   * @param instr Instruction char
   * @param instr_data Parallel data for the instr.
   * @param cell_num Current index of output tape.
   * @param cell_data Current data at output tape index.
   */
  void debug_log(int instr_num, char instr, int instr_data, int cell_num,
                 int cell_data);

  /**
   * Log a vector<int>
   *
   * @param log_vec Vector of int.
   */
  void debug_log(std::vector<int> log_vec);

  /**
   * Log a parallel string and vector<int>
   *
   * @param log_str String.
   * @param log_vec Vector of int.
   */
  void debug_log(std::string log_str, std::vector<int> log_vec);

  /**
   * Take in filepath, return string read in.
   *
   * @param filename Brainfuck file to read from.
   * @return string given by specified file.
   */
  std::string read_bf_file(std::string filename);

  /**
   * Clean instructions, handle pattern matching and condensing instrs.
   *
   * Final outputs stored in final_execution_string and
   * final_execution_data.
   *
   * @param values Container whose values are summed.
   * @return sum of `values`, or 0.0 if `values` is empty.
   */
  void preprocess(std::string raw_instructions);

  /**
   * Check whether syntax is valid.
   *
   * @param instructions Raw instruction string.
   */
  void syntax_check(std::string raw_instructions);

  /**
   * Locate duplicate instructions, condenses them.
   *
   * Number of duplicates stored in the parallel data vector.
   * Handled by preprocess()
   *
   * @param instructions String of instructions.
   */
  void coalesce_operands(std::string instructions);

  /**
   * Check whether "<" and ">" are balanced.
   *
   * Handled by pattern_match().
   *
   * @param str String to check against.
   * @return number of "<" or ">" counted.
   */
  int check_balance_traverse(std::string str);

  /**
   * Pattern match against instructions.
   *
   * Place pattern token and necessary data in place. Function
   * handled by coalesce_operands().
   *
   * @param instructions Uncondensed string of instructions to match on.
   * @return string of instructions with replaced patterns.
   */
  std::string pattern_match(std::string instructions);

  /**
   * Get and storice indices of matching loop brackets.
   *
   * Handled be preprocess(). Runs against the final condensed
   * instruction string.
   */
  void matching_loop();

  /**
   * Interprets the final string of instructions
   */
  void execute();
};

#endif