#pragma once
#include <cuda_runtime.h>

#ifdef __CUDACC__
  #define CUDA_GLOBAL __global__
#else
  #define CUDA_GLOBAL
#endif

// Device kernel: one thread per clause
CUDA_GLOBAL void gpu_propagate(
    int* clauseIndex, 
    int* literals, 
    int  numClauses, 
    int* results
);

// Host‐callable wrapper:
extern "C" void launch_gpu_propagate(
    int* clauseIndex,
    int* literals,
    int  numClauses,
    int* results,
    int  threads,
    int  blocks
);
