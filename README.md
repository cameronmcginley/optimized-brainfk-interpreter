# Optimized Brainfuck Interpreter
This interpreter is a simple project on how the brainfuck language can be optimized through the interpreter. The primary optimizations are condensing instructions and pattern matching.

Condensing instructions just means pushing multiple repetitive instructions into one instruction. For example, "+++" is interpreted as just "+", but adds 3 instead of 1 and repeating itself.

Pattern matching is a bit more complicated. Some common algorithms exist in brainfuck, such as addition or subtraction. With a bit of regex, we can find variations of these patterns and shortcut their operation to save on the number of instructions we execute. This is very valuable for optimization. Cutting down on loop iterations can save many instructions. In the case of this brainfuck mandelbrot calculation (https://github.com/erikdubbelboer/brainfuck-jit/blob/master/mandelbrot.bf), it saves many billions of instructions. Some documented brainfuck code patterns can be found here: https://esolangs.org/wiki/Brainfuck_algorithms

### Structure
This interpreter reads an instruction set from a file as a string. From there, this string is broken into two parallel vectors: one holding the instruction, and one holding a byte of data for that instruction. In terms of condensing instructions, that data byte holds the number of repetitions. Pattern matching sometimes requires more information, such as the locations of two numbers and where to store their sum. These bytes are stored in the instruction feed, immediately following the instruction token for the specific pattern, rather than the data vector. 

Data for pattern instructions are stored in the instruction feed mostly as a relic of the original implementation of this interpreter. After much playing around with data structures when first building this interpreter, a single vector of instructions and its needed data was found to run quite quickly. Splitting out data into a vector made things a bit more manageable though, but for more bytes of data we can still stick these in the instruction feed and hop over them to avoid accidental executions.

### Example Instruction Set
**Original**:

| > | > | > | > | + | + | + | > | + | > | + | + | [ | - | ] | < | [ | < | - | > | - | < | < | + | > | > | ] | 
| - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - |

**Optimized**:

| > | + | > | + | > | + | z | < | s | 1 | 2 |
| - | - | - | - | - | - | - | - | - | - | - |
| 4 | 3 | 1 | 1 | 1 | 2 | - | 1 | - | - | - |

In this case, there are two patterns: 'z' and 's'. 'z' stores no extra information, but 's' stores two bytes in the instruction feed. The 's' is a subtraction pattern. The current pointer points to the small number, the first byte after 's' indicates how far away the bigger number is, and the second byte indicates where to add the result.

## Compiling and Running
`g++ *.cpp -O3 -std=c++20 -o bf`

`./bf helloworld.bf`
