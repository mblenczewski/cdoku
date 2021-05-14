# cdoku

A sudoku solver written in C using Knuth's Algorithm X + dancing links

## Resources
Below are links to some of the resources I used to implement the solver

# Original Whitepaper + Wikipedia Page
The wikipedia page for [Algorithm X](https://en.wikipedia.org/wiki/Knuth's_Algorithm_X)
helped me get a birds-eye view of the algorithm, and pointed me further to the 
original whitepaper by Knuth ([whitepaper](https://arxiv.org/abs/cs/0011047)).

A [Reference C++ Implementation](https://github.com/Elementrix08/Sudoku/blob/master/Dancing-Links.cpp)
which allowed me to check my code, which was especially helpful during debugging 
of the `matrix_create` and `solve` functions (I ended up having an off-by-one 
bug in the former, and had messed up uncovering in the latter).

An [Excellent Primer](https://garethrees.org/2007/06/10/zendoku-generation/#figure-2)
on the use of Algorithm X to solve exact cover problems (sudoku specifically) 
helped me to again get a birds-eye view of the algorithm.

