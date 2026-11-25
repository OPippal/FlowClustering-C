# FlowClustering-C

A C command-line application that performs single-linkage cluster analysis on network flow data using weighted Euclidean distance.

## Key Concepts
* **Language:** C (C11)
* **Algorithms:** Single-linkage hierarchical clustering, sorting
* **Memory Management:** Manual dynamic memory allocation (`malloc`, `realloc`, `free`) without memory leaks

## How to Compile & Run
1. Clone the repository:
   ```bash
   git clone [https://github.com/OPippal/FlowClustering-C.git](https://github.com/OPippal/FlowClustering-C.git)
   cd FlowClustering-C
   Compile the program:
   gcc -std=c11 -Wall -Wextra -Werror flows.c -lm -o flows
   Run with an input file and target cluster count (e.g., 3 clusters with weights):
   ./flows data.txt 3 1.0 1.0 1.0 1.0