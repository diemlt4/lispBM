# lispBM [![Build Status](https://travis-ci.org/svenssonjoel/lispBM.svg?branch=master)](https://travis-ci.org/svenssonjoel/lispBM)

A lisp-like language (work in progress) implemented in C for 32-bit platforms.

## Purpose
1. Have fun.
2. Learn about lisp.
3. Learn about microcontrollers.
4. An interactive REPL for devboards.
5. ...

## Features
1. heap consisting of cons-cells with mark and sweep garbage collection.
2. Built-in functions: cons, car, cdr, eval, list, +, -, >, <, = and more.
3. Some special forms: Lambdas, closures, lets (letrecs), define and quote.
4. 28-Bit signed/unsigned integers and boxed 32-Bit Float, 32-Bit signed/unsigned values.
5. Arrays (in progress), string is an array. 
6. Compiles for, and runs on linux-x86 (builds 32bit library, runs on 32/64 bit).
7. Compiles for, and runs on Zynq 7000.
8. Compiles for, and runs on STM32f4. 
9. Compiles for, and runs on NRF52840.
10. Compiles for, and runs on ESP32.

## Experimental
1. Quasiquotation (needs more testing).
2. Concurrency (Work in progress).

## Documentation
LispBM's internals are documented as a series of [blog posts](http://svenssonjoel.github.io).

## Want to get involved and help out?
1. Are you interested in microcontrollers and programming languages?
2. You find it fun to mess around in C code with close to zero comments?
3. Then join in the fun. Lots to do, so little time!
4. Poke me by mail bo(dot)joel(dot)svensson(whirly-a)gmail(dot)com

## TODOs
1. (DONE) Write some tests that stresses the Garbage collector.
2. (DONE) Implement some "reference to X type", for uint32, int32. 
3. (DONE) Write a small library of useful hofs. 
4. (DONE) Improve handling of arguments in eval-cps. 
5. (DONE) Code improvements with simplicity, clarity  and readability in mind.
6. (DONE) Implement a small dedicated lisp reader/parser to replace MPC. MPC eats way to much memory for small platforms.
7. (DONE) Port to STM32f4 - 128K ram platform (will need big changes). (surely there will be some more bugs)
8. (DONE) Add STM32f4 example code (repl implementation)
9. (DONE) Port to nrf52840_pca10056 - 256k ram platform (same changes as above).
10. (DONE) Reduce size of builtins.c and put platform specific built in functions elsewhere. (Builtins.c will be removed an replaced by fundamentals.c) 
11. (DONE) Implement 'progn' facility.
12. (DONE) Remove the "gensym" functionality havent found a use for it so far and it only complicates things.
13. (DONE) Add NRF52 example repl to repository
14. (DONE) Update all example REPLs after adding quasiquotation
15. Test all example REPLs after addition of quasiquotation
16. Implement some looping structure for speed or just ease of use. 
17. Be much more stringent on checking of error conditions etc.
18. The parser allocates heap memory, but there are no interfacing with the GC there.


## Compile for linux (Requires 32bit libraries. May need something like "multilib" on a 64bit linux)
1. Build the library: `make`

2. Build the repl: `cd repl-cps` and then `make`

3. Run the repl: `./repl`

Blog: https://svenssonjoel.github.io/pages/lispbm_zephyros_repl/index.html
