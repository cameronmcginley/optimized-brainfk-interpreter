// BFCompiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <stack>
#include <string>
#include <chrono>
#include <regex>
#include <unordered_map>
using namespace std::chrono;

enum instructions {
    traverse_right,
    traverse_left,
    increment,
    decrement,
    loop_open,
    loop_close,
    output,
    input,
    pattern_zero,
    pattern_substract,
    pattern_move,
    dead_instr, // For temp vector set, don't move to final vector
};

instructions key_to_hash(char key) {
    if (key == '>') return traverse_right;
    if (key == '<') return traverse_left;
    if (key == '+') return increment;
    if (key == '-') return decrement;
    if (key == '[') return loop_open;
    if (key == ']') return loop_close;
    if (key == '.') return output;
    if (key == ',') return input;
}

struct Instruction {
    instructions key;

    // Depicts different variations for pattern matches
    std::string variation;

    // For condensable instructions
    int repeat;

    // For loop brackets
    int moveto;

    // Various helpful vars for patterns, hash map too slow
    int dist;
    int dist_to_big;
    int dist_to_ans;
};

class Tape {
    std::vector<Instruction> temp_instruction_set;
    std::vector<Instruction> final_instruction_set;
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

    void floop_open() {
        // Check if should enter loop (current not 0)
        if (tape[curr_tape_index] > 0) {
            if (debug) log("Loop entered");
        }
        // else, continue to after loop, find matching pair
        else {
            curr_instruction_index = final_instruction_set[curr_instruction_index].moveto;
            curr_instruction_index--; // Avoid instruction increment in execute()

            if (debug) log("Loop skipped");
        }
    }

    void floop_close() {
        // Check if should jump back (current not 0)
        if (tape[curr_tape_index] > 0) {
            curr_instruction_index = final_instruction_set[curr_instruction_index].moveto;
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

        // Convert raw instructions to vector of Instructions
        for (char c : raw_instructions) {
            Instruction new_instr;
            new_instr.key = key_to_hash(c);
            new_instr.repeat = 1;
            temp_instruction_set.push_back(new_instr);
        }

        // Still pattern match on the raw string for regex
        // But write to the temp_instruction vector
        pattern_match(raw_instructions);

        // Goes through temp instr vector, adds to counter if duplicates found
        // in a row
        std::cout << "YOYO";
        coalesce_operands();

        std::cout << "YOYO";
        //create_final_instr();

        matching_loop();
    }

    // Takes in command string, iterates through it moving commands into vector
    // Handles condensing duplicate operations (e.g. +++), and patter matching, e.g. "[-]"
    void coalesce_operands() {
        const std::vector<instructions> CONDENSABLE_INSTRS = { traverse_right, traverse_left, increment, decrement };

        for (int i = 0; i < temp_instruction_set.size(); i++) {

            // If it matches former key, add to vec.tops repeat and don't add this new one
            // to final instr vector
            if (final_instruction_set.size() > 0
                && final_instruction_set.back().key == temp_instruction_set[i].key
                && std::find(CONDENSABLE_INSTRS.begin(), CONDENSABLE_INSTRS.end(), temp_instruction_set[i].key) != CONDENSABLE_INSTRS.end()) {
                final_instruction_set.back().repeat++;
                continue;
            }

            // Add new instruction to final instr vec
            final_instruction_set.push_back(temp_instruction_set[i]);
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
    void pattern_match(std::string raw_instructions) {
        // Sometimes the instr will save data into the next removed char instead of 0
        std::vector<std::pair<std::string, instructions>> patterns = { 
            {"\\[-\\]", pattern_zero},
            //Adds/moves
            {"\\[->+\\+<+\\]", pattern_move}, //Simple trade (any number of < or >), must check < and > match for each match
            {"\\[>+\\+<+-\\]", pattern_move},
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
            {"\\[<+->+-<+\\+>+\\]", pattern_substract},
            {"\\[   >+-<+   <+\\+>+        \\]", pattern_substract},
            {"\\[   <+->+   >+\\+<+        \\]", pattern_substract},
            {"\\[   >+-<+   >+\\+<+        \\]", pattern_substract},
        };

        std::vector<int> index_matches;
        std::regex rx;
        //std::vector<int> extra_bytes;

        for (auto pattern : patterns) {
            rx = pattern.first;
            index_matches.clear();

            auto words_begin = std::sregex_iterator(raw_instructions.begin(), raw_instructions.end(), rx);
            auto words_end = std::sregex_iterator();

            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                std::smatch match = *i;
                std::string match_str = match.str();

                // If it's an s, check for matching < and >
                // [<-> - <<<<<<+>>>>>>]
                if (pattern.second == pattern_substract) {
                    // Split the match into 2 substrs, before and after the 2nd '-'
                    // Check for balanced <> in each one
                    size_t pos = match_str.find("-");
                    pos = match_str.find("-", pos + 1);

                    std::string half_one = match_str.substr(0, pos);
                    std::string half_two = match_str.substr(pos, std::string::npos);
                    
                    //Check halves
                    int dist_to_big = check_balance_traverse(half_one);
                    int dist_to_ans = check_balance_traverse(half_two);

                    // Returns 0 if not balanced, don't overwrite anything
                    if (dist_to_big == 0 || dist_to_ans == 0) continue;

                    // Write the distances to the instr in temp vector
                    temp_instruction_set[i->position()].dist_to_big = dist_to_big;
                    temp_instruction_set[i->position()].dist_to_ans = dist_to_ans;
                }

                // If it's a 't' (trade), check for matching < and >
                if (pattern.second == pattern_move) {
                    int dist = check_balance_traverse(match_str);

                    if (dist == 0) continue;

                    temp_instruction_set[i->position()].dist = dist;
                }

                // Replace the key of the first instruction in the temp vector
                // With the key defined in pair above
                temp_instruction_set[i->position()].key = pattern.second;

                // For all other instructions in the pattern, replace key with '0'
                for (int j = 1; j < match_str.size(); j++) {
                    temp_instruction_set[i->position() + j].key = dead_instr;
                }



                //// Replace the word with key and 0s
                //instructions[i->position()] = pattern.second;
                //for (int j = 1; j < match_str.size(); j++) {
                //    instructions[i->position() + j] = '0';
                //}

                //// Write extra bytes if needed
                //for (int j = 0; j < extra_bytes.size(); j++) {
                //    if (extra_bytes[j] != 0) {
                //        instructions[i->position() + j + 1] = extra_bytes[j];
                //    }
                //}
            }
        }

        std::cout << raw_instructions << std::endl;
        //return instructions;
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
            if (final_instruction_set[i].key == loop_open) {
                st.push({ '[', i });
            }

            if (final_instruction_set[i].key == loop_close) {
                // Bad code check
                if (st.size() == 0) {
                    std::cout << "Error: Missing opening bracket.";
                    exit(0);
                }

                int opener_index = st.top().second;
                int closer_index = i;

                // Swap to correct moveto indices
                final_instruction_set[closer_index].moveto = opener_index + 1;
                final_instruction_set[opener_index].moveto = closer_index + 1;

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
            for (Instruction c : final_instruction_set) {
                std::cout << c.key << ":" << c.repeat << std::endl;
            }
            std::cout << std::endl;
        }
    }

    void execute() {
        long long int instr_counter = 0;

        Instruction instr;
        for (curr_instruction_index; curr_instruction_index < final_instruction_set.size(); curr_instruction_index++) {
            instr = final_instruction_set[curr_instruction_index];
            //std::cout << command.instruction << "\n";

            switch (instr.key) {
            case pattern_substract: {
                if (tape[curr_tape_index] == 0) break;

                // ONLY HANDLES DEFAULT SCENARIO
                int small_val = tape[curr_tape_index];
                tape[curr_tape_index] = 0;

                // Num stored in instruction
                int move_to_big = final_instruction_set[curr_instruction_index].dist_to_big;

                int big_val = tape[curr_tape_index - move_to_big];
                tape[curr_tape_index - move_to_big] -= small_val;

                int answer_val = big_val - small_val;
                if (answer_val == 0) answer_val = 1; // Always moves 1 minimum

                int move_to_answer = final_instruction_set[curr_instruction_index].dist_to_ans;
                // Adds to the cell, not replace
                tape[curr_tape_index - move_to_answer] += answer_val;

                break;
            }
            case pattern_move: {
                // This takes number of current loc, sets to 0, and adds to the pointed loc
                // pointed loc is bytes to the right specified by the byte after 't'
                int val = tape[curr_tape_index];
                int moves = final_instruction_set[curr_instruction_index].dist;

                tape[curr_tape_index] = 0;
                tape[curr_tape_index + +moves] += val;

                break;
            }
            case pattern_zero:
                tape[curr_tape_index] = 0;
                break;
            case traverse_right:
                if (do_fast) {
                    curr_tape_index += instr.repeat;
                }
                else {
                    traverse_next(instr.repeat);
                }
                break;
            case traverse_left:
                if (do_fast) {
                    curr_tape_index -= instr.repeat;
                }
                else {
                    traverse_prev(instr.repeat);
                }
                break;
            case increment:
                if (do_fast) {
                    tape[curr_tape_index] += instr.repeat;
                }
                else {
                    increment_data(instr.repeat);
                }
                break;
            case decrement:
                if (do_fast) {
                    tape[curr_tape_index] -= instr.repeat;
                }
                else {
                    decrement_data(instr.repeat);
                }
                break;
            case input:
                getchar();
                break;
            case loop_open:
                floop_open();
                break;
            case loop_close:
                floop_close();
                break;
            case output:
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

