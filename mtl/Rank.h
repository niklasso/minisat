/******************************************************************************************[Rank.h]
Copyright (c) 2009-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef Minisat_Rank_h
#define Minisat_Rank_h

#include "mtl/IntTypes.h"
#include "mtl/Vec.h"

namespace Minisat {

//=================================================================================================
// OVERVIEW: This file implements a bitvector-datastructure with support for a ranking function. 
// The rank of a bit position 'p' corresponds to the number of ones that occurs before 'p'. This
// implementation follows closely the paper [1].
//
// The integer bit-population-count was taken from the web-page "Bit Twiddling Hacks" [2] which 
// contains lots of neat tricks (that are in the public domain) collected by Sean Eron Anderson.
// 
// NOTE: the datastructure is designed for 64-bit word-size, and although the principle extends to
// other sizes, there are several aspects (block size, level-2-rank reads) that has to be tweaked
// for a particular word-size. In other words, changing to other sizes is not entirely obvious and
// this is the reason that I decided not to parametrize the implementation with respect to this.

// [1] "Broadword Implementation of Rank/Select Queries" by Sebastiano Vigna
// [2] http://www-graphics.stanford.edu/~seander/bithacks.html
// 
//=================================================================================================
// Helper functions:
//

template<class T>
static inline int bitCount(T v)
{
    static const int CHAR_BIT = 8;                                // Assumes 8-bit characters.
    v = v - ((v >> 1) & (T)~(T)0/3);                              // temp
    v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);         // temp
    v = (v + (v >> 4)) & (T)~(T)0/255*15;                         // temp
    return (T)(v * ((T)~(T)0/255)) >> (sizeof(v) - 1) * CHAR_BIT; // count
}

static inline void printBits(uint64_t x)
{
    for (int i = 0; i < 7; i++){
        for (int j = 0; j < 9; j++)
            printf("%d", (bool)((x >> (i*9 + j))&1));
        printf(" ");
    }
    printf("%d", (bool)((x >> 63)&1));
}


//=================================================================================================
// Helper classes:
//

class WordRankBasic
{
    uint64_t x;
 public:
    void     set (int b){ x |= 1ULL << b; }
    bool     get (int b) const { return (x >> b)&1; }
    unsigned rank(int b) const {
        // Take away bits [63..b]:
        uint64_t _bits = x << (63-b);
        uint64_t bits  = _bits << 1;
        
        assert(b > 0 || bits == 0);

        // Bit-Population count of the rest:
        int sum = 0;
        for (; bits > 0; bits >>=1)
            sum += bits&1;
        return sum;
    }

    uint64_t peek() const { return x; }
};


class WordRankNative64
{
    uint64_t x;
 public:
    void     set (int b){ x |= 1ULL << b; }
    bool     get (int b) const { return (x >> b)&1; }
    unsigned rank(int b) const {
        // Take away bits [63..b]:
        uint64_t mask = (1ULL << b)-1;
        uint64_t bits = x & mask;

        // Bit-Population count of the rest:
        return bitCount(bits);
    }

    uint64_t peek() const { return x; }
};


class WordRankNative32
{
    uint32_t x[2];
 public:
    WordRankNative32() { x[0] = 0; x[1] = 0; }
    void     set (int b){ 
        x[0] |= !(b / 32) << (b % 32);
        x[1] |=  (b / 32) << (b % 32);
    }
    bool     get (int b) const { return (x[b / 32] >> (b % 32)) & 1; }
    unsigned rank(int b) const {
        // Take away bits [63..b]:
        uint64_t mask = (1ULL << b)-1;

        uint32_t bits[2];
        bits[0] = x[0] & (uint32_t)mask;
        bits[1] = x[1] & (uint32_t)(mask >> 32);

        // Bit-Population count of the rest:
        return bitCount(bits[0]) + bitCount(bits[1]);
    }

    // uint64_t peek() const { return x; }
};


class BlockRankBasic
{
    uint16_t word_ranks[8];

 public:
    BlockRankBasic(){
        for (int i = 0; i < 8; i++)
            word_ranks[i] = 0; }

    void increase(int w){
        assert(w > 0); 
        assert(w < 8);
        word_ranks[w]++; }

    void copy(int w1, int w2){
        assert(w1 >= 0); 
        assert(w1 <  8);
        assert(w2 >= 0); 
        assert(w2 <  8);
        word_ranks[w2] = word_ranks[w1]; }
        
    unsigned rank(int w) const { 
        assert(w >= 0); 
        assert(w < 8);
        return word_ranks[w]; }
};


class BlockRankByte
{
    uint8_t ranks[8];

    unsigned read(int w) const { 
        int      t = -(1 <= w);
        unsigned x = ranks[w] << 1 | (ranks[0] >> w)&1;
        return x & t; }

    void write(int w, unsigned val){
        assert(w > 0);
        ranks[w] = val >> 1;
        uint8_t mask = 1<<w;
        ranks[0] = (ranks[0] & ~mask) | (-(val&1) & mask);
    }

 public:
    BlockRankByte(){
        for (int i = 0; i < 8; i++)
            ranks[i] = 0; }

    void     increase(int w)      { write(w, read(w)+1); }
    void     copy(int w1, int w2) { write(w2, read(w1)); }
    unsigned rank(int w) const    { return read(w); }
};


class BlockRankNative64
{
    uint64_t data;

    unsigned read (int word) const {
        int      t         = word-1;
        unsigned mask      = ((1 << 9)-1);
        unsigned cancel    = (t >> (sizeof(t)*8-4))&8; 
        unsigned rot_right = (t + cancel)*9;

        // NOTE: If t >= 0, cancel = 0 and and rot_right = t*9.
        //       Otherwise, if t = -1, then cancel = 7 and rot_right = (-1 + 8)*9 = 7 * 9 = 63.
        //       Then since bit 63 of data is always zero, data >> 63 gives the wanted result 0.

        return (data >> rot_right) & mask;
    }
    void write(int word, unsigned val){
        // printf("*** WRITING %2d to word %3d ***********************************************\n", val, word);
        // printf(" val         = "); printBits(val); printf("\n");
        // printf(" data-before = "); printBits(data); printf("\n");
        // uint64_t before = data;

        assert(word > 0);
        assert(val < 512);
        unsigned t     = word-1;
        uint64_t mask  = ((1ULL << 9)-1) << (t*9);
        uint64_t clear = data & ~mask;
        data = clear | ((uint64_t)val << (t*9));

        // printf(" mask        = "); printBits(mask); printf("\n");
        // printf(" clear       = "); printBits(clear); printf("\n");
        // printf(" data-after  = "); printBits(data); printf("\n");
        assert(read(word) == val);
    }

public:
    BlockRankNative64() : data(0) {}

    void     increase(int w)      { write(w, read(w)+1); }
    void     copy(int w1, int w2) { write(w2, read(w1)); }
    unsigned rank(int w) const    { return read(w); }
};


template<typename IndexT, typename BlockRankT>
class PrefixedBlockRank
{
    IndexT     prev_block;
    BlockRankT current;

 public:
    PrefixedBlockRank(IndexT prev) : prev_block(prev) {}

    void increase(int w)         { current.increase(w); }
    int  rank    (int w) const   { return prev_block + current.rank(w); }
    void copy    (int w1, int w2){ current.copy(w1, w2); }
};


// Set default implementation based on 64-bit architechture test. This may not be very
// precise on non-gcc compilers.
#ifdef __LP64__
typedef BlockRankNative64 DefaultBlockRank;
typedef WordRankNative64  DefaultWordRank;
#else
typedef BlockRankByte     DefaultBlockRank;
typedef WordRankNative32  DefaultWordRank;
#endif


//=================================================================================================
// Main-class: Bit-vector with support for ranking function:
//

template<
    typename IndexT     = uint32_t,
    typename BlockRankT = DefaultBlockRank,
    typename WordRankT  = DefaultWordRank
    >
class RankBitVec
{
    vec<WordRankT>                              bits;
    vec<PrefixedBlockRank<IndexT, BlockRankT> > blocks;
    int                                         next_bit;
    IndexT                                      total_bits;
 public:
    RankBitVec();

    int    size      ()         const;
    bool   operator[](IndexT x) const;
    IndexT rank      (IndexT x) const;

    void   push      (bool bit);
    void   clear     (bool dealloc = false);
    void   moveTo    (RankBitVec& to);
};


template<typename IndexT, typename BlockRankT, typename WordRankT>
inline int RankBitVec<IndexT, BlockRankT, WordRankT>::size() const
{
    return (bits.size()-1) * 64 + next_bit;
}


template<typename IndexT, typename BlockRankT, typename WordRankT>
bool RankBitVec<IndexT, BlockRankT, WordRankT>::operator[](IndexT x) const
{
    IndexT word = x / 64;
    IndexT bit  = x % 64;

    assert(word >= 0);
    assert(word < (IndexT)bits.size());

    return bits[word].get(bit);
}


template<typename IndexT, typename BlockRankT, typename WordRankT>
IndexT RankBitVec<IndexT, BlockRankT, WordRankT>::rank(IndexT x) const
{
    IndexT word  = x / 64;
    IndexT bit   = x % 64;
    IndexT block = word / 8;
    IndexT bword = word % 8;

    assert(word  >= 0);
    assert(word   < (IndexT)bits.size());
    assert(block >= 0);
    assert(block  < (IndexT)blocks.size());

    WordRankT w          = bits[word];
    IndexT    block_rank = blocks[block].rank(bword);

    // printf(" --- RANK:\n");
    // printf(" >>> index = %d, word = %d, bit = %d, block = %d, bword = %d\n",
    //        x, word, bit, block, bword);
    // 
    // printf(" >>> block_rank = %d, w.rank(%d) = %d\n", 
    //        block_rank, bit, w.rank(bit));
    // printf(" >>> word.peek() = %"PRIu64", block_rank = %d, w.rank(%d) = %d\n", 
    //        w.peek(), block_rank, bit, w.rank(bit));

    return block_rank + w.rank(bit);
}


template<typename IndexT, typename BlockRankT, typename WordRankT>
void RankBitVec<IndexT, BlockRankT, WordRankT>::push(bool bit)
{
    int word_in_block = (IndexT)(bits.size()-1) % 8;
    int bit_in_word   = next_bit++;

    // TODO: explain the following to some extent.
    if (bit){
        total_bits++;

        if (word_in_block < 8-1)
            blocks.last().increase(word_in_block+1);

        bits.last().set(bit_in_word);
        // printf(" --- value = %"PRIu64", bit = %d\n", bits.last().peek(), bit_in_word);
    }

    if (next_bit == 64){
        // printf(" --- new word\n");
        next_bit = 0;
        bits.push();

        if (word_in_block < 8-2)
            blocks.last().copy(word_in_block+1, word_in_block+2);

        if (word_in_block == 8-1)
            blocks.push(PrefixedBlockRank<IndexT, BlockRankT>(total_bits));
    }
}


template<typename IndexT, typename BlockRankT, typename WordRankT>
void RankBitVec<IndexT, BlockRankT, WordRankT>::clear(bool dealloc)
{
    bits  .clear(dealloc);
    blocks.clear(dealloc);
    bits  .push ();
    blocks.push (PrefixedBlockRank<IndexT, BlockRankT>(0));

    total_bits = next_bit = 0;
}


template<typename IndexT, typename BlockRankT, typename WordRankT>
void RankBitVec<IndexT, BlockRankT, WordRankT>::moveTo(RankBitVec& to)
{
    to.total_bits = total_bits;
    to.next_bit   = next_bit;
    bits  .moveTo(to.bits);
    blocks.moveTo(to.blocks);
    clear();
}


template<typename IndexT, typename BlockRankT, typename WordRankT>
RankBitVec<IndexT, BlockRankT, WordRankT>::RankBitVec()
{
    clear();
}


//=================================================================================================
}

#endif
