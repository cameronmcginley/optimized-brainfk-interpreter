#include "Interpreter.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <regex>
#include <stack>

Interpreter::Interpreter() : curr_instruction_index(0), curr_tape_index(0) {
  tape.resize(3000);
}

void Interpreter::log(std::string log_str) {
  std::cout << log_str << std::endl;
}

void Interpreter::log(int instr_num, char instr, int instr_data, int cell_num,
                      int cell_data) {
  std::cout << "Instr #" << instr_num << ": Instr= " << instr << " "
            << ": Instr Data=" << instr_data << ": Cell #" << cell_num
            << ": Cell Data=" << cell_data << std::endl;
}

void Interpreter::log(std::vector<int> log_vec) {
  std::cout << ": ";
  for (auto item : log_vec) {
    std::cout << item << " ";
  }
  std::cout << std::endl;
}

void Interpreter::log(std::string log_str, std::vector<int> log_vec) {
  std::cout << "Instruction : Data" << std::endl;
  for (int i = 0; i < log_str.size(); i++) {
    std::cout << log_str[i] << " : " << log_vec[i] << std::endl;
  }
}

std::string Interpreter::read_bf_file(std::string filename) {
  // Maybe run file checks in the future
  std::ifstream ifs(filename);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));

  return content;
}

void Interpreter::preprocess(std::string raw_instructions) {
  if (debug) log("Preprocessing...");

  // Takes in the raw instruction set
  // Remove all whitespace
  raw_instructions.erase(std::remove_if(raw_instructions.begin(),
                                        raw_instructions.end(), ::isspace),
                         raw_instructions.end());
  if (debug) log("Whitespace removed!");

  execution_data.resize(30000);
  execution_data2.reserve(30000);

  // syntax_check(raw_instructions);

  raw_instructions = pattern_match(raw_instructions);

  // Converts the raw string into file instruction set of vector<Cell>
  coalesce_operands(raw_instructions);

  // Remove all 0s from execution_str and corresponding execution_data
  if (debug) log("Building new instruction string and data vector...");

  for (int i = 0; i < execution_str.size(); i++) {
    if (execution_str[i] != '0') {
      execution_string2 += execution_str[i];
      execution_data2.push_back(execution_data[i]);
    }
  }

  matching_loop();

  if (debug) log("Build complete!");
  if (debug) log("\nInstructions following condensing (w/o data vector):");
  if (debug) log(execution_string2);
}

// Checks for valid character set
// If invalid found, throw error and kill program
void Interpreter::syntax_check(std::string raw_instructions) {
  if (debug) log("Checking syntax...");

  const std::string allowed_chars = "+-><.,[]";

  for (char c : raw_instructions) {
    if (allowed_chars.find(c) == std::string::npos) {
      std::cout << "Invalid syntax: " << c;
      exit(0);
    }
  }

  if (debug) log("Syntax check complete!");
}

// Takes in command string, iterates through it moving commands into vector
// Handles condensing duplicate operations (e.g. +++), and patter matching, e.g.
// "[-]" Will error if dupes > 255 (1 byte)
void Interpreter::coalesce_operands(std::string instructions) {
  if (debug) log("Condensing instructions...");

  const std::string condensed_chars = "+-><";

  int former_instr_index = NULL;
  char former_instr = NULL;

  // Coalesce operations into same string, replace duplicates with '0'
  // Except for first duplciate, replace with the counter
  for (int i = 0; i < instructions.size(); i++) {
    // Skip if deleted by pattern matcher
    if (instructions[i] == '0') {
      continue;
    }

    // Match dupes
    if (instructions[i] == former_instr) {
      if (condensed_chars.find(instructions[i]) != std::string::npos) {
        // Set self to 0, then add 1 to matching execution data
        instructions[i] = '0';
        execution_data[former_instr_index]++;

        continue;
      }
    }

    // No match, overwrite the vars
    former_instr = instructions[i];
    former_instr_index = i;
  }

  execution_str = instructions;

  if (debug) log("Condensing complete!");
}

int Interpreter::check_balance_traverse(std::string str) {
  if (debug) log("Checking '<>' match...");

  auto next_count = std::count(str.begin(), str.end(), '>');
  auto prev_count = std::count(str.begin(), str.end(), '<');

  // If not, don't replace anything
  if (next_count != prev_count) {
    return 0;
  }

  // Store this number after 't' to indicate how many spaces away the trade is
  return next_count;

  if (debug) log("Match confirmed!");
}

// Takes in string of raw instructions, searches for common patterns
// Replaces the first instr of that pattern with a unique key and replaces
// the rest of the pattern with a 0
// Patterns:
// https://esolangs.org/wiki/Brainfuck_algorithms#if_.28x.29_.7B_code_.7D
// https://gist.github.com/roachhd/dce54bec8ba55fb17d3a
// https://en.wikipedia.org/wiki/Brainfuck
// Mandelbrot:
// https://github.com/erikdubbelboer/brainfuck-jit/blob/master/mandelbrot.bf
std::string Interpreter::pattern_match(std::string instructions) {
  if (debug) log("Beginning pattern matching...");

  // Sometimes the instr will save data into the next removed char instead of 0
  std::vector<std::pair<std::string, char>> patterns = {
      // Zero out current cell
      {"\\[-\\]", 'z'},

      // Adds/moves to target cell (current becomes 0)
      // To right:
      // ex: [->>>+<<<]
      {"\\[->+\\+<+\\]", 't'},
      //{"\\[>+\\+<+-\\]", ''},
      // To left:
      // ex: [-<<<+>>>]
      {"\\[-<+\\+>+\\]", 'l'},
      //{"\\[<+\\+>+-\\]", ''},

      // Subtraction with answer cell
      // ex: [<->-<<<<<<+>>>>>>]
      // Instruction steps:
      // 0: Open loop on small number
      // 1: Move to big number and decrement
      // 2: move to small number and decrement it
      // 3: Move to answer tile and increment
      // 4: Move to small number and end loop
      // Formats:
      // Number of < and > are balanced
      // [<->-<<<<<<+>>>>>>] (big on left of small, answer to left)
      // [>-<->>>>>>+<<<<<<] (big on right of small, answer to right)
      {"\\[<+->+-<+\\+>+\\]", 's'},
      //{"\\[   >+-<+   <+\\+>+        \\]", '1'},
      //{"\\[   <+->+   >+\\+<+        \\]", '2'},
      //{"\\[   >+-<+   >+\\+<+        \\]", '3'},

      // Subtraction no answer cell
      // ex: [-<<<->>>]
      {"\\[-<+->+\\]", 'm'},
      //[->>>-<<<]
      {"\\[->+-<+\\]", 'n'},
  };

  std::vector<int> index_matches;
  std::regex rx;
  std::vector<int> extra_bytes;

  for (auto pattern : patterns) {
    rx = pattern.first;
    index_matches.clear();

    auto words_begin =
        std::sregex_iterator(instructions.begin(), instructions.end(), rx);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
      std::smatch match = *i;
      std::string match_str = match.str();
      extra_bytes.clear();

      if (debug) log("Pattern found: " + match_str);

      // If it's an s, check for matching < and >
      // [<-> - <<<<<<+>>>>>>]
      if (pattern.second == 's') {
        // Split the match into 2 substrs, before and after the 2nd '-'
        // Check for balanced <> in each one
        size_t pos = match_str.find("-");
        pos = match_str.find("-", pos + 1);

        std::string half_one = match_str.substr(0, pos);
        std::string half_two = match_str.substr(pos, std::string::npos);

        // Check halves
        //  Store this number after 's' to indicate how many to move
        int extra_byte1 = check_balance_traverse(half_one);
        int extra_byte2 = check_balance_traverse(half_two);

        // Returns 0 if not balanced, don't overwrite anything
        if (extra_byte1 == 0 || extra_byte2 == 0) continue;

        extra_bytes.push_back(extra_byte1);
        extra_bytes.push_back(extra_byte2);
      }

      // If it's a 't' (trade), check for matching < and >
      if (pattern.second == 't' || pattern.second == 'l' ||
          pattern.second == 'm' || pattern.second == 'n') {
        // Store this number after 't' to indicate how many spaces away the
        // trade is
        extra_bytes.push_back(check_balance_traverse(match_str));
      }

      // Replace the word with key and 0s
      instructions[i->position()] = pattern.second;
      for (int j = 1; j < match_str.size(); j++) {
        instructions[i->position() + j] = '0';
      }

      // Write extra bytes if needed
      for (int j = 0; j < extra_bytes.size(); j++) {
        if (extra_bytes[j] != 0) {
          instructions[i->position() + j + 1] = extra_bytes[j];
        }
      }
    }
  }

  if (debug) log("Pattern matching complete!");
  return instructions;
}

void Interpreter::matching_loop() {
  if (debug) log("Matching bracket indices...");

  // Scan through program to find matching loops
  std::stack<std::pair<char, int>> st;

  for (int i = 0; i < execution_string2.size(); i++) {
    // Store its own index in its pair
    // When closing is found, swap their indices
    if (execution_string2[i] == '[') {
      st.push({'[', i});
    }

    if (execution_string2[i] == ']') {
      // Bad code check
      if (st.size() == 0) {
        std::cout << "Error: Missing opening bracket.";
        exit(0);
      }

      int opener_index = st.top().second;
      int closer_index = i;

      execution_data2[opener_index] = closer_index + 1;
      execution_data2[closer_index] = opener_index + 1;

      st.pop();
    }
  }

  if (st.size() != 0) {
    std::cout << "Error: Missing closing bracket.";
    exit(0);
  }

  if (debug) log("Bracket pairs matched!");
}

void Interpreter::execute() {
  if (debug) log("\nExecuting...\nFinal instructions and data:");
  if (debug) log(execution_string2, execution_data2);
  if (debug) log("\n");

  long long int instr_counter = 0;

  for (curr_instruction_index;
       curr_instruction_index < execution_string2.size();
       curr_instruction_index++) {
    if (debug)
      log(curr_instruction_index, execution_string2[curr_instruction_index],
          execution_data2[curr_instruction_index], curr_tape_index,
          +tape[curr_tape_index]);

    switch (execution_string2[curr_instruction_index]) {
      case 's': {
        if (tape[curr_tape_index] == 0) break;

        // ONLY HANDLES DEFAULT SCENARIO
        int small_val = tape[curr_tape_index];
        tape[curr_tape_index] = 0;

        // Num stored in instruction
        int move_to_big = execution_string2[curr_instruction_index + 1];

        int big_val = tape[curr_tape_index - +move_to_big];
        tape[curr_tape_index - +move_to_big] -= +small_val;

        int answer_val = +big_val - +small_val;
        if (answer_val == 0) answer_val = 1;  // Always moves 1 minimum

        int move_to_answer = execution_string2[curr_instruction_index + 2];
        // Adds to the cell, not replace
        tape[curr_tape_index - +move_to_answer] += +answer_val;

        // Skip the data instrs
        curr_instruction_index += 2;

        break;
      }
      case 't': {
        // This takes number of current loc, sets to 0, and adds to the pointed
        // loc pointed loc is bytes to the right specified by the byte after 't'
        int val = tape[curr_tape_index];
        int moves = execution_string2[curr_instruction_index + 1];

        tape[curr_tape_index] = 0;
        tape[curr_tape_index + +moves] += val;

        curr_instruction_index++;

        break;
      }
      case 'l': {
        // Exact same as t, but move left
        int val = tape[curr_tape_index];
        int moves = execution_string2[curr_instruction_index + 1];

        tape[curr_tape_index] = 0;
        tape[curr_tape_index - +moves] += val;

        curr_instruction_index++;

        break;
      }
      case 'z':
        tape[curr_tape_index] = 0;
        break;
      case 'm': {
        // m moves to the left
        int val = tape[curr_tape_index];
        int moves = execution_string2[curr_instruction_index + 1];

        tape[curr_tape_index] = 0;
        tape[curr_tape_index - +moves] -= val;

        curr_instruction_index++;

        break;
      }
      case 'n': {
        // same as m, but moves to right
        int val = tape[curr_tape_index];
        int moves = execution_string2[curr_instruction_index + 1];

        tape[curr_tape_index] = 0;
        tape[curr_tape_index + +moves] -= val;

        curr_instruction_index++;

        break;
      }
      case '>':
        curr_tape_index += execution_data2[curr_instruction_index] + 1;
        // Make sure to skip the data instr after it's used
        // i++;
        break;
      case '<':
        curr_tape_index -= execution_data2[curr_instruction_index] + 1;
        // i++;
        break;
      case '+':
        tape[curr_tape_index] += execution_data2[curr_instruction_index] + 1;
        // i++;
        break;
      case '-':
        tape[curr_tape_index] -= execution_data2[curr_instruction_index] + 1;
        // i++;
        break;
      case ',': {
        int n;
        std::cout << "Input: ";
        std::cin >> n;
        std::cout << std::endl;
        tape[curr_tape_index] = n;
        break;
      }
      case '[': {
        if (tape[curr_tape_index] == 0) {
          curr_instruction_index = execution_data2[curr_instruction_index];
          curr_instruction_index--;  // Avoid instruction increment
        }
        break;
      }
      case ']': {
        if (tape[curr_tape_index] > 0) {
          curr_instruction_index = execution_data2[curr_instruction_index];
          curr_instruction_index--;  // Avoid instruction increment
        }
        break;
      }
      case '.':
        if (debug) {
          debug_store_output += tape[curr_tape_index];
        } else {
          std::cout << tape[curr_tape_index];
        }
        break;
      default:
        break;
    }

    instr_counter++;
  }

  // The output is stored if debug is on, print after execution
  if (debug) std::cout << std::endl << debug_store_output << std::endl;

  std::cout << "\nTotal instructions: " << instr_counter << std::endl;
}
