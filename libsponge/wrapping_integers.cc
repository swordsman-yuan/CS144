#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
        /*1.get the raw value of isn*/
        uint32_t RawISN = isn.raw_value();

        /*2.get the offset of n, rewind if exceeding 2^32 - 1*/
        uint32_t NOffset = (n & ((1ul << 32) - 1)) + RawISN;    // n & 0xffffffff + RawISN

        /*3.construct the Result*/
        WrappingInt32 Result(NOffset);
        return Result;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
        /*build a base number*/
        const uint64_t Base = (1ul << 32);

        /*1.calculate the real offset between n and isn*/
        // ATTENTION: the value of Increment can be negative
        int32_t Increments = operator-(n, isn);  
        uint32_t NOffset = (Increments < 0) ? Increments + Base : Increments;

        /*2.adjust Result into the same inteval as checkpoint*/
        uint64_t ResultBase = checkpoint >> 32;
        uint32_t CheckPointOffset = checkpoint & (Base - 1);    // checkpoint & 0xffffffff
        uint64_t Result = 0;

        // condition is something like : [0   N     CKPT     2^32-1]
        if(NOffset < CheckPointOffset)          
        {
                uint32_t PresentDiff = CheckPointOffset - NOffset;
                uint32_t NextDiff = NOffset + (Base - CheckPointOffset);
                /*the ResultBase cannot be larger than 0xfffffffe*/
                if(ResultBase < (Base - 1) && PresentDiff > NextDiff)
                        ResultBase++;       // jump to the next inteval
        }
        // condition is something like : [0   CKPT     N     2^32-1]
        else if(NOffset > CheckPointOffset)     
        {
                uint32_t PresentDiff = NOffset - CheckPointOffset;
                uint32_t LastDiff = CheckPointOffset + (Base - NOffset);
                /*the ResultBase cannot be smaller than 1*/
                if(ResultBase > 0 && PresentDiff > LastDiff)
                        ResultBase--;
        }
        
        /*3.stitch the ResultBase and NOffset*/
        Result = Result | (ResultBase << 32) | NOffset;
        return Result;
}
