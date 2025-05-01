// tests/test_gpu_vs_cpu.cpp
#include <cassert>
#include <vector>
#include <random>
#include <iostream>
#include <zlib.h>

#include "minisat/core/Solver.h"
#include "minisat/core/Dimacs.h"
#include "gpu/gpu_propagate.h"
#include <cuda_runtime.h>

using namespace Minisat;

// simple cuda‐error checker
static void cudaCheck(const char *msg) {
    if (auto e = cudaGetLastError(); e != cudaSuccess) {
        std::cerr << "CUDA error (" << msg << "): "
                  << cudaGetErrorString(e) << "\n";
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: test_gpu_vs_cpu <cnf-file>\n";
        return 1;
    }

    // 1) Parse CNF
    Solver S;
    gzFile in = gzopen(argv[1], "rb");
    if (!in) { perror("gzopen"); return 1; }
    parse_DIMACS(in, S, /*strict=*/false);
    gzclose(in);

    auto const& CL = S.getClauses();       // vec<CRef>
    auto&       CA = S.getCA();            // ClauseAllocator
    int m = CL.size();
    int nVars = S.nVars();

    // flatten into CSR
    int L = 0;
    for (int cid = 0; cid < m; cid++)
        L += CA[ CL[cid] ].size();

    std::vector<int> h_clauseIndex(m+1), h_literals;
    h_literals.reserve(L);
    {
        int offs = 0;
        for (int cid = 0; cid < m; cid++) {
            h_clauseIndex[cid] = offs;
            auto& c = CA[ CL[cid] ];
            for (int j = 0; j < c.size(); j++) {
                Lit lit = c[j];
                h_literals.push_back(lit.x);
            offs += c.size();
            }
        }
        h_clauseIndex[m] = offs;
    }

    // 2) Allocate & upload clause arrays once
    int *d_clauseIndex=nullptr, *d_literals=nullptr, *d_results=nullptr;
    cudaMalloc(&d_clauseIndex, (m+1)*sizeof(int)); cudaCheck("malloc idx");
    cudaMalloc(&d_literals,    L    *sizeof(int)); cudaCheck("malloc lits");
    cudaMalloc(&d_results,     m    *sizeof(int)); cudaCheck("malloc res");
    cudaMemcpy(d_clauseIndex, h_clauseIndex.data(), (m+1)*sizeof(int),
               cudaMemcpyHostToDevice); cudaCheck("memcpy idx→GPU");
    cudaMemcpy(d_literals,    h_literals.data(),    L   *sizeof(int),
               cudaMemcpyHostToDevice); cudaCheck("memcpy lits→GPU");

    // 3) Prepare storage for assignments & results
    std::vector<int> h_assign(nVars), h_results(m);
    int* d_assignments = nullptr;
    cudaMalloc(&d_assignments, nVars * sizeof(int)); cudaCheck("malloc assign");

    // 4) Random engine
    std::mt19937 rng(123456);
    std::uniform_int_distribution<int> bit(0,1);

    const int TRIALS = 100;
    for (int t = 0; t < TRIALS; t++) {
        // 4a) build a random assignment
        for (int v = 0; v < nVars; v++)
            h_assign[v] = bit(rng);

        // 4b) upload it
        cudaMemcpy(d_assignments,
                   h_assign.data(),
                   nVars * sizeof(int),
                   cudaMemcpyHostToDevice);
        cudaCheck("memcpy assign→GPU");

        // 4c) launch the kernel
        int threads = 256;
        int blocks  = (m + threads - 1) / threads;
        launch_gpu_propagate(
            d_clauseIndex,
            d_literals,
            d_assignments,
            m,
            d_results,
            threads,
            blocks
        );
        cudaCheck("launch");
        cudaDeviceSynchronize(); cudaCheck("sync");

        // 4d) download results
        cudaMemcpy(h_results.data(), d_results,
                   m * sizeof(int),
                   cudaMemcpyDeviceToHost);
        cudaCheck("memcpy res→host");

        // 4e) CPU‐side check under the SAME assignment
        for (int cid = 0; cid < m; cid++) {
            const auto& c = CA[ CL[cid] ];
            bool cpu_sat = false;
            for (int j = 0; j < c.size(); j++) {
                Lit lit = c[j];
                int val = h_assign[var(lit)];
                // lit is true if (val == 1 && !sign(lit)) or (val == 0 && sign(lit))
                bool lit_true = (val == 1) ^ sign(lit);
                if (lit_true) { cpu_sat = true; break; }
            }
            bool gpu_sat = (h_results[cid] != 0);
            if (cpu_sat != gpu_sat) {
                std::cerr << "❌ MISMATCH at trial " << t
                          << ", clause " << cid
                          << ": CPU=" << cpu_sat
                          << " GPU=" << gpu_sat << "\n";
                return 1;
            }
        }
    }

    std::cout << "✅ All " << TRIALS
              << " random trials passed for " << m
              << " clauses / " << nVars
              << " vars.\n";

    // cleanup
    cudaFree(d_clauseIndex);
    cudaFree(d_literals);
    cudaFree(d_results);
    cudaFree(d_assignments);
    return 0;
}
