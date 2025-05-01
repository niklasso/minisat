#include "gpu/gpu_propagate.h"

// GPU kernel
__global__
void gpu_propagate(
    int* clauseIndex,
    int* literals,
    int  numClauses,
    int* results
) {
    int cid = blockIdx.x * blockDim.x + threadIdx.x;
    if (cid >= numClauses) return;

    // STUB: mark everything as “needs CPU” (0)
    // later you’ll write real logic here
    results[cid] = 0;
}

// Host wrapper, compiled by NVCC
extern "C"
void launch_gpu_propagate(
    int* clauseIndex,
    int* literals,
    int  numClauses,
    int* results,
    int  threads,
    int  blocks
) {
    gpu_propagate<<<blocks, threads>>>(
        clauseIndex, literals, numClauses, results
    );
    cudaDeviceSynchronize();
}
