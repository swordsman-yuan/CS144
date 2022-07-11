#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>

/*define a structure to describe arriving non-contigious sub string*/
    struct _SubString{
      std::pair<size_t, size_t> _Inteval;         /*the sub string inteval, []*/
      std::string _Str;                           /*sub string*/
      bool _Eof;                                  /*whether the substring is the end of entire stream*/

      /*ctor*/
      _SubString() :  _Inteval(), 
                      _Str(),
                      _Eof(){}
      _SubString(std::pair<size_t, size_t> Inteval, std::string Str, bool Eof) :  _Inteval(Inteval), 
                                                                                  _Str(Str),
                                                                                  _Eof(Eof){}
    };

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
    private:
    /*
      used to keep and manage the arriving non-contigious substring
      the total size of substring can not exceed capacity - bytestream's size
    */
    std::vector <_SubString> _ArrivalStrManager;  

    /*next index the reassembler is waiting for*/
    uint64_t _NextIndex;

    // Your code here -- add private members as necessary.
    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

    /*the standard of sorting the vector*/
    static bool cmpFunction(const _SubString& A, const _SubString& B){
        return  A._Inteval.first < B._Inteval.first ? true  :
                A._Inteval.first > B._Inteval.first ? false :
                A._Inteval.second < B._Inteval.second;
    }
    
    /*private function added by zheyuan*/
    //! \brief merge the pre-existed substring and the newly-arrived substring
    std::vector<_SubString> mergeInteval();

  public:
    // public member function added by zheyuan
    //! \brief get the next index the reassembler is wating for
    uint64_t getNextIndex() const {return _NextIndex;}

    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
