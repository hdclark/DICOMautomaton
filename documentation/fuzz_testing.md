
# Overview

`DICOMautomaton` can be fuzz-tested with `AFL` (i.e., "American Fuzzy Lop"). `AFL` works best when it can instrument a
program that exposes the code that needs to be tested. Fuzzing can degrade system performance and potentially consume
all available memory, so it is best done in a constrained environment.

Note that fuzz testing seems to work best for unit-testing routines that load and parse files. Loading routines should
gracefully handle (known) bad inputs. The ideal program for a fuzz test is one that reads a file, parses it, performs
some validation or basic test of functionality, and exits. If a known problem is detected (i.e., the structure of the
file is not valid) then the program should ignore them and possibly exit normally -- it should not throw an exception.

## Preparing a Constrained Environment

A base system can be prepared via:

    $>  sudo docker run -it --rm --memory 8G --memory-swap 8G --cpus=4 debian:stable

Build dependencies can be prepared via:

    $>  apt-get -y update && 
        apt-get -y install build-essential git cmake libboost-dev libcgal-dev libeigen3-dev afl

## `Explicator`

Compile and install the explicator library. 

    $>  ( cd ~/ && 
          mkdir -p ~/dcma && 
          cd ~/dcma && 
          git clone https://github.com/hdclark/explicator explicator && 
          cd explicator && 
          mkdir build && 
          cd build && 
          cmake -E env CC=afl-gcc CXX=afl-g++ \
          cmake \
            -DCMAKE_INSTALL_PREFIX=/usr \
            -DCMAKE_BUILD_TYPE=Release \
            ../ && 
          make VERBOSE=1 && 
          make install )

This will also build and install some simple test programs that could be fuzzed:

    explicator_print_weights_thresholds
    explicator_cross_verify
    explicator_lexicon_dogfooder
    explicator_translate_string_levenshtein
    explicator_translate_string_jarowinkler
    explicator_translate_string
    explicator_translate_string_all_general

However, these programs will generally **intentionally** throw exceptions when the input lexicon can not be read. `AFL`
treats these as crashes equivalent to situations where an **unintentional** crash occurs. So it's best to write a
separate, minimal test program that:

  1. Directly exercises the code to be fuzzed, and
  2. Reads all inputs from a file, and
  3. Catches all anticipated exceptions and exits the program normally.

## Preparing Test Files

The `explicator_translate_string` program accepts two arguments: a lexicon file and a string to translate.
A simple test lexicon can be prepared. It will be fuzzed.

    $>  cd / &&
        mkdir -p ~/dcma && 
        cd ~/dcma && 
        mkdir -p afl_explicator &&
        cd afl_explicator &&
        mkdir -p testcase_dir &&
        printf 'junk : junk\n' > testcase_dir/A &&
        printf 'junk : junk\ntest : test\n' > testcase_dir/B &&
        printf 'left : right\nright : left\n' > testcase_dir/C

## Launching Fuzzing

    $>  cd ~/dcma/afl_explicator &&
        afl-fuzz -i testcase_dir -o findings_dir -- \
          explicator_translate_string @@ test_string

The running fuzzer shows a test interface so the runtime can be monitored. It looks like this:

                 american fuzzy lop 2.52b (explicator_translate_string)
    
    ┌─ process timing ─────────────────────────────────────┬─ overall results ─────┐
    │        run time : 0 days, 0 hrs, 3 min, 5 sec        │  cycles done : 0      │
    │   last new path : 0 days, 0 hrs, 0 min, 11 sec       │  total paths : 335    │
    │ last uniq crash : none seen yet                      │ uniq crashes : 0      │
    │  last uniq hang : none seen yet                      │   uniq hangs : 0      │
    ├─ cycle progress ────────────────────┬─ map coverage ─┴───────────────────────┤
    │  now processing : 179 (53.43%)      │    map density : 2.09% / 3.08%         │
    │ paths timed out : 0 (0.00%)         │ count coverage : 3.08 bits/tuple       │
    ├─ stage progress ────────────────────┼─ findings in depth ────────────────────┤
    │  now trying : bitflip 2/1           │ favored paths : 47 (14.03%)            │
    │ stage execs : 92/191 (48.17%)       │  new edges on : 91 (27.16%)            │
    │ total execs : 218k                  │ total crashes : 0 (0 unique)           │
    │  exec speed : 1192/sec              │  total tmouts : 0 (0 unique)           │
    ├─ fuzzing strategy yields ───────────┴───────────────┬─ path geometry ────────┤
    │   bit flips : 12/4704, 8/4488, 4/4440               │    levels : 3          │
    │  byte flips : 0/564, 0/540, 0/494                   │   pending : 311        │
    │ arithmetics : 12/31.4k, 0/5883, 0/954               │  pend fav : 28         │
    │  known ints : 1/3005, 0/14.3k, 0/21.4k              │ own finds : 332        │
    │  dictionary : 0/0, 0/0, 0/0                         │  imported : n/a        │
    │       havoc : 295/123k, 0/0                         │ stability : 100.00%    │
    │        trim : 4.24%/138, 0.00%                      ├────────────────────────┘
    └─────────────────────────────────────────────────────┘          [cpu000: 34%]
    
Specific inputs that produce crashes will be available in `findings_dir/`.

# Conclusion

This example demonstrates how to run `AFL` on a simple test case.

