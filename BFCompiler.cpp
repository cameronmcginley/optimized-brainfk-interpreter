// BFCompiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <stack>
#include <string>
#include <chrono>
#include <regex>
using namespace std::chrono;

struct Cell {
    char instruction;
    // Counter is used as the moveto index for loop brackets
    int counter;
};

class Tape {
    // Each instruction is a pair of (instruction, moveto index)
    // the 2nd index only useful for loops
    std::vector<Cell> final_instruction_set;
    int curr_instruction_index;

    std::vector<char> tape;
    int curr_tape_index;

    bool debug = false;
    //do_fast disabled allows you to go against the strict brainfuck ruleset of 3000
    bool do_fast = true;

    std::string output_store = "";

public:
    Tape() : curr_instruction_index(0), curr_tape_index(0) {
        tape.resize(3000);
    }

    void log(std::string log_string) {
        std::cout << "Instr " << curr_instruction_index
            << ": Cell " << curr_tape_index
            << ": CellVal=" << +tape[curr_tape_index]
            << ": " << log_string << std::endl;
    }

    void traverse_next(int repeat) {
        if (debug) log("Traverse right to cell " + std::to_string(curr_tape_index + 1));

        // Check if exceeds 3000
        if (curr_tape_index + repeat > 3000) {
            std::cout << "Out of memory";
            exit(0);
        }

        curr_tape_index += repeat;
    }

    void traverse_prev(int repeat) {
        if (debug) log("Traverse right to cell " + std::to_string(curr_tape_index - 1));

        // Check if goes below 0
        if (curr_tape_index - repeat < 0) {
            std::cout << "Out of memory";
            exit(0);
        }

        curr_tape_index -= repeat;
    }

    void output_byte() {
        // If debug enabled, store the output and print it manually later
        if (!debug) {
            std::cout << tape[curr_tape_index];
        }
        else {
            output_store += tape[curr_tape_index];
        }
    }

    void increment_data(int repeat) {
        if (debug) log("Increment by " + std::to_string(repeat));

        tape[curr_tape_index] += repeat;
    }

    void decrement_data(int repeat) {
        if (debug) log("Decrement by " + std::to_string(repeat));

        tape[curr_tape_index] -= repeat;
    }

    void getchar() {
        int n;
        std::cout << "Input: ";
        std::cin >> n;
        std::cout << std::endl;
        tape[curr_tape_index] = n;
    }

    void loop_open() {
        // Check if should enter loop (current not 0)
        if (tape[curr_tape_index] > 0) {
            if (debug) log("Loop entered");
        }
        // else, continue to after loop, find matching pair
        else {
            curr_instruction_index = final_instruction_set[curr_instruction_index].counter;
            curr_instruction_index--; // Avoid instruction increment in execute()

            if (debug) log("Loop skipped");
        }
    }

    void loop_close() {
        // Check if should jump back (current not 0)
        if (tape[curr_tape_index] > 0) {
            curr_instruction_index = final_instruction_set[curr_instruction_index].counter;
            curr_instruction_index--; // Avoid instruction increment in execute()

            if (debug) log("Loop repeated");
        }
        // else, continue to next command
        else {
            if (debug) log("Loop ended");
        }
    }

    void preprocess(std::string raw_instructions) {
        if (debug) std::cout << "Preprocessing...\n\n";

        // Takes in the raw instruction set
        // Remove all whitespace
        raw_instructions.erase(std::remove_if(raw_instructions.begin(), raw_instructions.end(), ::isspace), raw_instructions.end());

        syntax_check(raw_instructions);

        // Converts the raw string into file instruction set of vector<Cell>
        coalesce_operands(raw_instructions);

        matching_loop();
    }

    // Takes in command string, iterates through it moving commands into vector
    // Handles condensing duplicate operations (e.g. +++), and patter matching, e.g. "[-]"
    void coalesce_operands(std::string instructions) {
        const std::string condensed_chars = "+-><";

        instructions = pattern_match(instructions);

        // Push initial instruction
        Cell new_cell = { instructions[0], 1 };
        final_instruction_set.push_back(new_cell);

        for (int i = 1; i < instructions.size(); i++) {
            // Skip if deleted by pattern matcher
            if (instructions[i] == '0') {
                continue;
            }

            // Condense instr into former instr if match + allowed char
            if (final_instruction_set.back().instruction == instructions[i]
                && condensed_chars.find(instructions[i]) != std::string::npos) {
                final_instruction_set.back().counter++;
            }
            else {
                // Add new instruction
                // Counter defaults to 1, brackets will overwrite this
                Cell new_cell = { instructions[i], 1 };
                final_instruction_set.push_back(new_cell);
            }
        }
    }

    int check_balance_traverse(std::string str) {
        auto next_count = std::count(str.begin(), str.end(), '>');
        auto prev_count = std::count(str.begin(), str.end(), '<');

        // If not, don't replace anything
        if (next_count != prev_count) {
            return 0;
        }

        // Store this number after 't' to indicate how many spaces away the trade is
        return next_count;
    }

    // Takes in string of raw instructions, searches for common patterns
    // Replaces the first instr of that pattern with a unique key and replaces
    // the rest of the pattern with a 0
    std::string pattern_match(std::string instructions) {
        // Sometimes the instr will save data into the next removed char instead of 0
        std::vector<std::pair<std::string, char>> patterns = { 
            {"\\[-\\]", 'z'},
            //Adds/moves
            {"\\[->+\\+<+\\]", 't'}, //Simple trade (any number of < or >), must check < and > match for each match
            {"\\[>+\\+<+-\\]", 't'},
            //Copy
            //Needs aux + destination cells

            //Subtraction
            //[<->-<<<<<<+>>>>>>]
            //0: Open loop on small number
            //1: Move to big number and decrement
            //2: move to small number and decrement it
            //3: Move to answer tile and increment
            //4: Move to small number and end loop
            // Formats: 
            // Number of < and > are balanced
            // [<->-<<<<<<+>>>>>>] (big on left of small, answer to left)
            // [>-<->>>>>>+<<<<<<] (big on right of small, answer to right)
            {"\\[<+->+-<+\\+>+\\]", 's'},
            {"\\[   >+-<+   <+\\+>+        \\]", '1'},
            {"\\[   <+->+   >+\\+<+        \\]", '2'},
            {"\\[   >+-<+   >+\\+<+        \\]", '3'},
        };

        std::vector<int> index_matches;
        std::regex rx;
        int extra_byte;
        int extra_byte2;

        for (auto pattern : patterns) {
            rx = pattern.first;
            index_matches.clear();

            auto words_begin = std::sregex_iterator(instructions.begin(), instructions.end(), rx);
            auto words_end = std::sregex_iterator();

            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                std::smatch match = *i;
                std::string match_str = match.str();

                // Set this if need an extra byte of data
                extra_byte = 0;
                extra_byte2 = 0;

                // If it's an s, check for matching < and >
                // [<-> - <<<<<<+>>>>>>]
                if (pattern.second == 's') {
                    // Split the match into 2 substrs, before and after the 2nd '-'
                    // Check for balanced <> in each one
                    size_t pos = match_str.find("-");
                    pos = match_str.find("-", pos + 1);

                    std::string half_one = match_str.substr(0, pos);
                    std::string half_two = match_str.substr(pos, std::string::npos);
                    //std::cout << match_str << " : " << match_str.substr(0, pos) << std::endl;
                    //std::cout << match_str << " : " << match_str.substr(pos, std::string::npos) << std::endl;
                    
                    //Check halves
                    // Store this number after 's' to indicate how many to move
                    extra_byte = check_balance_traverse(half_one);
                    extra_byte2 = check_balance_traverse(half_two);

                    // Returns 0 if not balanced, don't overwrite anything
                    if (extra_byte == 0 || extra_byte2 == 0) continue;
                }

                // If it's a 't' (trade), check for matching < and >
                if (pattern.second == 't') {
                    auto next_count = std::count(match_str.begin(), match_str.end(), '>');
                    auto prev_count = std::count(match_str.begin(), match_str.end(), '<');

                    // If not, don't replace anything
                    if (next_count != prev_count) {
                        continue;
                    }

                    // Store this number after 't' to indicate how many spaces away the trade is
                    extra_byte = next_count;
                }

                // Replace the word with key and 0s
                instructions[i->position()] = pattern.second;
                for (int j = 1; j < match_str.size(); j++) {
                    instructions[i->position() + j] = '0';
                }

                if (extra_byte != 0) {
                    instructions[i->position() + 1] = extra_byte;
                }

                if (extra_byte2 != 0) {
                    instructions[i->position() + 2] = extra_byte2;
                }
            }
        }

        //std::cout << instructions << std::endl;
        return instructions;
    }

    // Checks for valid character set
    // If invalid found, throw error and kill program
    void syntax_check(std::string raw_instructions) {
        const std::string allowed_chars = "+-><.,[]";

        for (char c : raw_instructions) {
            if (allowed_chars.find(c) == std::string::npos) {
                std::cout << "Invalid syntax: " << c;
                exit(0);
            }
        }
    }

    void matching_loop() {
        // Scan through code, find matching loop sets
        std::stack<std::pair<char, int>> st;

        // Modify the pairs in final_insutrction_set for the loop indices
        for (int i = 0; i < final_instruction_set.size(); i++) {
            // Store its own index in its pair
            // When closing is found, swap their indices
            if (final_instruction_set[i].instruction == '[') {
                st.push({ '[', i });
            }

            if (final_instruction_set[i].instruction == ']') {
                // Bad code check
                if (st.size() == 0) {
                    std::cout << "Error: Missing opening bracket.";
                    exit(0);
                }

                int opener_index = st.top().second;
                int closer_index = i;

                // Swap to correct moveto indices
                final_instruction_set[closer_index].counter = opener_index + 1;
                final_instruction_set[opener_index].counter = closer_index + 1;

                // Remove the pair
                st.pop();
            }
        }

        if (st.size() != 0) {
            std::cout << "Error: Missing closing bracket.";
            exit(0);
        }

        if (debug) {
            std::cout << "Final instruction set: (instruction:counter/moveto index)\n";
            for (Cell c : final_instruction_set) {
                std::cout << c.instruction << ":" << c.counter << std::endl;
            }
            std::cout << std::endl;
        }
    }

    void execute() {
        long long int instr_counter = 0;

        Cell command;
        for (curr_instruction_index; curr_instruction_index < final_instruction_set.size(); curr_instruction_index++) {
            command = final_instruction_set[curr_instruction_index];
            //std::cout << command.instruction << "\n";

            switch (command.instruction) {
            case 's': {
                if (tape[curr_tape_index] == 0) break;

                // ONLY HANDLES DEFAULT SCENARIO
                int small_val = tape[curr_tape_index];
                tape[curr_tape_index] = 0;

                // Num stored in instruction
                int move_to_big = final_instruction_set[curr_instruction_index + 1].instruction;

                int big_val = tape[curr_tape_index - +move_to_big];
                tape[curr_tape_index - +move_to_big] -= +small_val;

                int answer_val = +big_val - +small_val;
                if (answer_val == 0) answer_val = 1; // Always moves 1 minimum

                int move_to_answer = final_instruction_set[curr_instruction_index + 2].instruction;
                // Adds to the cell, not replace
                tape[curr_tape_index - +move_to_answer] += +answer_val;

                break;
            }
            case 't': {
                // This takes number of current loc, sets to 0, and adds to the pointed loc
                // pointed loc is bytes to the right specified by the byte after 't'
                int val = tape[curr_tape_index];
                int moves = final_instruction_set[curr_instruction_index+1].instruction;

                tape[curr_tape_index] = 0;
                tape[curr_tape_index + +moves] += val;

                break;
            }
            case 'z':
                tape[curr_tape_index] = 0;
                break;
            case '>':
                if (do_fast) {
                    curr_tape_index += command.counter;
                }
                else {
                    traverse_next(command.counter);
                }
                break;
            case '<':
                if (do_fast) {
                    curr_tape_index -= command.counter;
                }
                else {
                    traverse_prev(command.counter);
                }
                break;
            case '+':
                if (do_fast) {
                    tape[curr_tape_index] += command.counter;
                }
                else {
                    increment_data(command.counter);
                }
                break;
            case '-':
                if (do_fast) {
                    tape[curr_tape_index] -= command.counter;
                }
                else {
                    decrement_data(command.counter);
                }
                break;
            case ',':
                getchar();
                break;
            case '[':
                loop_open();
                break;
            case ']':
                loop_close();
                break;
            case '.':
                output_byte();
                break;
            default:
                break;
            }

            instr_counter++;
        }

        if (true) {
            std::cout << "\nTotal instructions: " << instr_counter << std::endl;
            //print_tape();
        }
    }

    void print_info() {

    }

    void print_tape() {
        int temp_index = 0;

        for (int i = 0; i < 3000; i++) {
            std::cout << i << ": " << +tape[i] << "\n";
        }
    }
};

//BF Rules:
// > increment ptr
// < decrement ptr
// + increment
// - decrement
// . output the byte
// , accept one byte of input, store at current
// [ if current is 0,jump to after ], else move on
// ] if current not 0, jump to command after matching [, else move on

int main()
{
    auto start = high_resolution_clock::now();
    //std::cout << "Hello World!\n";

    Tape BFTape;

    std::string temppp = "+++++++++++++[->++>>>+++++>++>+<<<<<<]>>>>>++++++>--->>>>>>>>>>+++++++++++++++[[ >>>>>>>>>]+[<<<<<<<<<]>>>>>>>>>-]+[>>>>>>>>[-]>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>[-]+ <<<<<<<+++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>>>+>>>>>>>>>>>>>>>>>>>>>>>>>> >+<<<<<<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+[>>>>>>[>>>>>>>[-]>>]<<<<<<<<<[<<<<<<<<<]>> >>>>>[-]+<<<<<<++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>>+<<<<<<+++++++[-[->>> >>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>>+<<<<<<<<<<<<<<<<[<<<<<<<<<]>>>[[-]>>>>>>[>>>>> >>[-<<<<<<+>>>>>>]<<<<<<[->>>>>>+<<+<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>> [>>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+<<+<<<+<<]>>>>>>>>]<<<<<<<<<[<<<<<<< <<]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+<<+<<<<<]>>>>>>>>>+++++++++++++++[[ >>>>>>>>>]+>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+[ >+>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>[-<<<<+>>>>]<<<<[->>>>+<<<<<[->>[ -<<+>>]<<[->>+>>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<< <<[>[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<< [>[-]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<<]<+<<<<<<<<<]>>>>> >>>>[>+>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>>[-<<<<<+>>>>>]<<<<<[->>>>>+ <<<<<<[->>>[-<<<+>>>]<<<[->>>+>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>> >>>>>>>]<<<<<<<<<[>>[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<<]>>[->>>>>>>>>+<<<<<<<<<]<< +>>>>>>>>]<<<<<<<<<[>[-]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<< <]<+<<<<<<<<<]>>>>>>>>>[>>>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>>>>>>>>>>>>> >>>>>>>>>>>>>>>>>>>>>>>]>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[[>>>> >>>>>]<<<<<<<<<-<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+>>>>>>>>>>>>>>>>>>>>>+<<<[<<<<<< <<<]>>>>>>>>>[>>>[-<<<->>>]+<<<[->>>->[-<<<<+>>>>]<<<<[->>>>+<<<<<<<<<<<<<[<<<<< <<<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>[-<<<<->>>>]+<<<<[->>>>-<[-<<<+>>>]<<<[-> >>+<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<< <<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]<<<<<<<[->+>>>-<<<<]>>>>>>>>>+++++++++++++++++++ +++++++>>[-<<<<+>>>>]<<<<[->>>>+<<[-]<<]>>[<<<<<<<+<[-<+>>>>+<<[-]]>[-<<[->+>>>- <<<<]>>>]>>>>>>>>>>>>>[>>[-]>[-]>[-]>>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-]>>>>>>[>>>>> [-<<<<+>>>>]<<<<[->>>>+<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>[-<<<<<<<< <+>>>>>>>>>]>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[[>>>>>>>>>]+>[- ]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+[>+>>>>>>>>]<<< <<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>>[-<<<<<+>>>>>]<<<<<[->>>>>+<<<<<<[->>[-<<+>>]< <[->>+>+<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<<<[>[->>>> >>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<[>[-]<->>> [-<<<+>[<->-<<<<<<<+>>>>>>>]<[->+<]>>>]<<[->>+<<]<+<<<<<<<<<]>>>>>>>>>[>>>>>>[-< <<<<+>>>>>]<<<<<[->>>>>+<<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>+>>>>>>>> ]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>>[-<<<<<+>>>>>]<<<<<[->>>>>+<<<<<<[->>[-<<+ >>]<<[->>+>>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<<<[> [->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<[>[- ]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<<]<+<<<<<<<<<]>>>>>>>>> [>>>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ]>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+> >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>]>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>++++++++ +++++++[[>>>>>>>>>]<<<<<<<<<-<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+[>>>>>>>>[-<<<<<<<+ >>>>>>>]<<<<<<<[->>>>>>>+<<<<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>>[ -]>>>]<<<<<<<<<[<<<<<<<<<]>>>>+>[-<-<<<<+>>>>>]>[-<<<<<<[->>>>>+<++<<<<]>>>>>[-< <<<<+>>>>>]<->+>]<[->+<]<<<<<[->>>>>+<<<<<]>>>>>>[-]<<<<<<+>>>>[-<<<<->>>>]+<<<< [->>>>->>>>>[>>[-<<->>]+<<[->>->[-<<<+>>>]<<<[->>>+<<<<<<<<<<<<[<<<<<<<<<]>>>[-] +>>>>>>[>>>>>>>>>]>+<]]+>>>[-<<<->>>]+<<<[->>>-<[-<<+>>]<<[->>+<<<<<<<<<<<[<<<<< <<<<]>>>>[-]+>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<<<]>>>>>>>>]<<<<<<<<< [<<<<<<<<<]>>>>[-<<<<+>>>>]<<<<[->>>>+>>>>>[>+>>[-<<->>]<<[->>+<<]>>>>>>>>]<<<<< <<<+<[>[->>>>>+<<<<[->>>>-<<<<<<<<<<<<<<+>>>>>>>>>>>[->>>+<<<]<]>[->>>-<<<<<<<<< <<<<<+>>>>>>>>>>>]<<]>[->>>>+<<<[->>>-<<<<<<<<<<<<<<+>>>>>>>>>>>]<]>[->>>+<<<]<< <<<<<<<<<<]>>>>[-]<<<<]>>>[-<<<+>>>]<<<[->>>+>>>>>>[>+>[-<->]<[->+<]>>>>>>>>]<<< <<<<<+<[>[->>>>>+<<<[->>>-<<<<<<<<<<<<<<+>>>>>>>>>>[->>>>+<<<<]>]<[->>>>-<<<<<<< <<<<<<<+>>>>>>>>>>]<]>>[->>>+<<<<[->>>>-<<<<<<<<<<<<<<+>>>>>>>>>>]>]<[->>>>+<<<< ]<<<<<<<<<<<]>>>>>>+<<<<<<]]>>>>[-<<<<+>>>>]<<<<[->>>>+>>>>>[>>>>>>>>>]<<<<<<<<< [>[->>>>>+<<<<[->>>>-<<<<<<<<<<<<<<+>>>>>>>>>>>[->>>+<<<]<]>[->>>-<<<<<<<<<<<<<< +>>>>>>>>>>>]<<]>[->>>>+<<<[->>>-<<<<<<<<<<<<<<+>>>>>>>>>>>]<]>[->>>+<<<]<<<<<<< <<<<<]]>[-]>>[-]>[-]>>>>>[>>[-]>[-]>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>[-< <<<+>>>>]<<<<[->>>>+<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[ [>>>>>>>>>]+>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+ [>+>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>[-<<<<+>>>>]<<<<[->>>>+<<<<<[->> [-<<+>>]<<[->>+>+<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<< <[>[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<[ >[-]<->>>[-<<<+>[<->-<<<<<<<+>>>>>>>]<[->+<]>>>]<<[->>+<<]<+<<<<<<<<<]>>>>>>>>>[ >>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>]> >>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>[-]>>>>+++++++++++++++[[>>>>>>>>>]<<<<<<<<<-<<<<< <<<<[<<<<<<<<<]>>>>>>>>>-]+[>>>[-<<<->>>]+<<<[->>>->[-<<<<+>>>>]<<<<[->>>>+<<<<< <<<<<<<<[<<<<<<<<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>[-<<<<->>>>]+<<<<[->>>>-<[- <<<+>>>]<<<[->>>+<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>> >>>>>>>]<<<<<<<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-<<<+>>>]<<<[->>>+>>>>>>[>+>>> [-<<<->>>]<<<[->>>+<<<]>>>>>>>>]<<<<<<<<+<[>[->+>[-<-<<<<<<<<<<+>>>>>>>>>>>>[-<< +>>]<]>[-<<-<<<<<<<<<<+>>>>>>>>>>>>]<<<]>>[-<+>>[-<<-<<<<<<<<<<+>>>>>>>>>>>>]<]> [-<<+>>]<<<<<<<<<<<<<]]>>>>[-<<<<+>>>>]<<<<[->>>>+>>>>>[>+>>[-<<->>]<<[->>+<<]>> >>>>>>]<<<<<<<<+<[>[->+>>[-<<-<<<<<<<<<<+>>>>>>>>>>>[-<+>]>]<[-<-<<<<<<<<<<+>>>> >>>>>>>]<<]>>>[-<<+>[-<-<<<<<<<<<<+>>>>>>>>>>>]>]<[-<+>]<<<<<<<<<<<<]>>>>>+<<<<< ]>>>>>>>>>[>>>[-]>[-]>[-]>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-]>[-]>>>>>[>>>>>>>[-<<<<< <+>>>>>>]<<<<<<[->>>>>>+<<<<+<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>+>[-<-<<<<+>>>> >]>>[-<<<<<<<[->>>>>+<++<<<<]>>>>>[-<<<<<+>>>>>]<->+>>]<<[->>+<<]<<<<<[->>>>>+<< <<<]+>>>>[-<<<<->>>>]+<<<<[->>>>->>>>>[>>>[-<<<->>>]+<<<[->>>-<[-<<+>>]<<[->>+<< <<<<<<<<<[<<<<<<<<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>[-<<->>]+<<[->>->[-<<<+>>>]< <<[->>>+<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]< <<<<<<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-<<<+>>>]<<<[->>>+>>>>>>[>+>[-<->]<[->+ <]>>>>>>>>]<<<<<<<<+<[>[->>>>+<<[->>-<<<<<<<<<<<<<+>>>>>>>>>>[->>>+<<<]>]<[->>>- <<<<<<<<<<<<<+>>>>>>>>>>]<]>>[->>+<<<[->>>-<<<<<<<<<<<<<+>>>>>>>>>>]>]<[->>>+<<< ]<<<<<<<<<<<]>>>>>[-]>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+<<+<<<<<]]>>>>[-<<<<+> >>>]<<<<[->>>>+>>>>>[>+>>[-<<->>]<<[->>+<<]>>>>>>>>]<<<<<<<<+<[>[->>>>+<<<[->>>- <<<<<<<<<<<<<+>>>>>>>>>>>[->>+<<]<]>[->>-<<<<<<<<<<<<<+>>>>>>>>>>>]<<]>[->>>+<<[ ->>-<<<<<<<<<<<<<+>>>>>>>>>>>]<]>[->>+<<]<<<<<<<<<<<<]]>>>>[-]<<<<]>>>>[-<<<<+>> >>]<<<<[->>>>+>[-]>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+<<+<<<<<]>>>>>>>>>[>>>>>> >>>]<<<<<<<<<[>[->>>>+<<<[->>>-<<<<<<<<<<<<<+>>>>>>>>>>>[->>+<<]<]>[->>-<<<<<<<< <<<<<+>>>>>>>>>>>]<<]>[->>>+<<[->>-<<<<<<<<<<<<<+>>>>>>>>>>>]<]>[->>+<<]<<<<<<<< <<<<]]>>>>>>>>>[>>[-]>[-]>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>[-]>[-]>>>>>[>>>>>[-<<<<+ >>>>]<<<<[->>>>+<<<+<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>>[-<<<<<+>>>>> ]<<<<<[->>>>>+<<<+<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[[>>>> >>>>>]+>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+[>+>> >>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>[-<<<<+>>>>]<<<<[->>>>+<<<<<[->>[-<<+ >>]<<[->>+>>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>>>>>]<<<<<<<<<[> [->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<]>[->>>>>>>>>+<<<<<<<<<]<+>>>>>>>>]<<<<<<<<<[>[- ]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<<]<+<<<<<<<<<]>>>>>>>>> [>+>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>->>>>>[-<<<<<+>>>>>]<<<<<[->>>>>+<<<< <<[->>>[-<<<+>>>]<<<[->>>+>+<<<<]+>>>>>>>>>]<<<<<<<<[<<<<<<<<<]]>>>>>>>>>[>>>>>> >>>]<<<<<<<<<[>>[->>>>>>>>>+<<<<<<<<<]<<<<<<<<<<<]>>[->>>>>>>>>+<<<<<<<<<]<<+>>> >>>>>]<<<<<<<<<[>[-]<->>>>[-<<<<+>[<->-<<<<<<+>>>>>>]<[->+<]>>>>]<<<[->>>+<<<]<+ <<<<<<<<<]>>>>>>>>>[>>>>[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+>>>>>>>>>>>>>>>>> >>>>>>>>>>>>>>>>>>>]>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>+++++++++++++++[[>>>>>>>> >]<<<<<<<<<-<<<<<<<<<[<<<<<<<<<]>>>>>>>>>-]+>>>>>>>>>>>>>>>>>>>>>+<<<[<<<<<<<<<] >>>>>>>>>[>>>[-<<<->>>]+<<<[->>>->[-<<<<+>>>>]<<<<[->>>>+<<<<<<<<<<<<<[<<<<<<<<< ]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>[-<<<<->>>>]+<<<<[->>>>-<[-<<<+>>>]<<<[->>>+< <<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<<<]> >>>>>>>]<<<<<<<<<[<<<<<<<<<]>>->>[-<<<<+>>>>]<<<<[->>>>+<<[-]<<]>>]<<+>>>>[-<<<< ->>>>]+<<<<[->>>>-<<<<<<.>>]>>>>[-<<<<<<<.>>>>>>>]<<<[-]>[-]>[-]>[-]>[-]>[-]>>>[ >[-]>[-]>[-]>[-]>[-]>[-]>>>]<<<<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>[-]>>>>]<<<<<<<<< [<<<<<<<<<]>+++++++++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>+>>>>>>>>>+<<<<<<<< <<<<<<[<<<<<<<<<]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+[-]>>[>>>>>>>>>]<<<<< <<<<[>>>>>>>[-<<<<<<+>>>>>>]<<<<<<[->>>>>>+<<<<<<<[<<<<<<<<<]>>>>>>>[-]+>>>]<<<< <<<<<<]]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<[->>>>>>>+>>[>+>>>>[-<<<<->>>>]<<<<[->>> >+<<<<]>>>>>>>>]<<+<<<<<<<[>>>>>[->>+<<]<<<<<<<<<<<<<<]>>>>>>>>>[>>>>>>>>>]<<<<< <<<<[>[-]<->>>>>>>[-<<<<<<<+>[<->-<<<+>>>]<[->+<]>>>>>>>]<<<<<<[->>>>>>+<<<<<<]< +<<<<<<<<<]>>>>>>>-<<<<[-]+<<<]+>>>>>>>[-<<<<<<<->>>>>>>]+<<<<<<<[->>>>>>>->>[>> >>>[->>+<<]>>>>]<<<<<<<<<[>[-]<->>>>>>>[-<<<<<<<+>[<->-<<<+>>>]<[->+<]>>>>>>>]<< <<<<[->>>>>>+<<<<<<]<+<<<<<<<<<]>+++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>+<<< <<[<<<<<<<<<]>>>>>>>>>[>>>>>[-<<<<<->>>>>]+<<<<<[->>>>>->>[-<<<<<<<+>>>>>>>]<<<< <<<[->>>>>>>+<<<<<<<<<<<<<<<<[<<<<<<<<<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>>>>[-< <<<<<<->>>>>>>]+<<<<<<<[->>>>>>>-<<[-<<<<<+>>>>>]<<<<<[->>>>>+<<<<<<<<<<<<<<[<<< <<<<<<]>>>[-]+>>>>>>[>>>>>>>>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<<<]>>>>>>>>]<<<<<<< <<[<<<<<<<<<]>>>>[-]<<<+++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>-<<<<<[<<<<<<< <<]]>>>]<<<<.>>>>>>>>>>[>>>>>>[-]>>>]<<<<<<<<<[<<<<<<<<<]>++++++++++[-[->>>>>>>> >+<<<<<<<<<]>>>>>>>>>]>>>>>+>>>>>>>>>+<<<<<<<<<<<<<<<[<<<<<<<<<]>>>>>>>>[-<<<<<< <<+>>>>>>>>]<<<<<<<<[->>>>>>>>+[-]>[>>>>>>>>>]<<<<<<<<<[>>>>>>>>[-<<<<<<<+>>>>>> >]<<<<<<<[->>>>>>>+<<<<<<<<[<<<<<<<<<]>>>>>>>>[-]+>>]<<<<<<<<<<]]>>>>>>>>[-<<<<< <<<+>>>>>>>>]<<<<<<<<[->>>>>>>>+>[>+>>>>>[-<<<<<->>>>>]<<<<<[->>>>>+<<<<<]>>>>>> >>]<+<<<<<<<<[>>>>>>[->>+<<]<<<<<<<<<<<<<<<]>>>>>>>>>[>>>>>>>>>]<<<<<<<<<[>[-]<- >>>>>>>>[-<<<<<<<<+>[<->-<<+>>]<[->+<]>>>>>>>>]<<<<<<<[->>>>>>>+<<<<<<<]<+<<<<<< <<<]>>>>>>>>-<<<<<[-]+<<<]+>>>>>>>>[-<<<<<<<<->>>>>>>>]+<<<<<<<<[->>>>>>>>->[>>> >>>[->>+<<]>>>]<<<<<<<<<[>[-]<->>>>>>>>[-<<<<<<<<+>[<->-<<+>>]<[->+<]>>>>>>>>]<< <<<<<[->>>>>>>+<<<<<<<]<+<<<<<<<<<]>+++++[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>> +>>>>>>>>>>>>>>>>>>>>>>>>>>>+<<<<<<[<<<<<<<<<]>>>>>>>>>[>>>>>>[-<<<<<<->>>>>>]+< <<<<<[->>>>>>->>[-<<<<<<<<+>>>>>>>>]<<<<<<<<[->>>>>>>>+<<<<<<<<<<<<<<<<<[<<<<<<< <<]>>>>[-]+>>>>>[>>>>>>>>>]>+<]]+>>>>>>>>[-<<<<<<<<->>>>>>>>]+<<<<<<<<[->>>>>>>> -<<[-<<<<<<+>>>>>>]<<<<<<[->>>>>>+<<<<<<<<<<<<<<<[<<<<<<<<<]>>>[-]+>>>>>>[>>>>>> >>>]>[-]+<]]+>[-<[>>>>>>>>>]<<<<<<<<]>>>>>>>>]<<<<<<<<<[<<<<<<<<<]>>>>[-]<<<++++ +[-[->>>>>>>>>+<<<<<<<<<]>>>>>>>>>]>>>>>->>>>>>>>>>>>>>>>>>>>>>>>>>>-<<<<<<[<<<< <<<<<]]>>>]";
    //std::string temppp = "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.";

    BFTape.preprocess(temppp);
    BFTape.execute();

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    std::cout << "Time taken: "<< duration.count() << " microseconds" << std::endl;
}

