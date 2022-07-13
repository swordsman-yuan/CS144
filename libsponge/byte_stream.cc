#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

/*ctor for ByteStream*/
/*Attention: the Stream size is capacity+1 because a additional position is used for the judgment of full*/
ByteStream::ByteStream(const size_t capacity) : _error(false), _endin(false),
                                                _Stream(),
                                                _Capacity(capacity),
                                                _TotalWritten(0), _TotalRead(0){}

size_t ByteStream::write(const string &data) {
    
    /*1.calculate the remain space of stream*/
    size_t Remainder =  this->remaining_capacity();
    
    /*2.the byte can be written into stream is the smaller value of data.size() and the left space of stream*/
    size_t ByteWrite =  (data.size() >= Remainder) ? Remainder : data.size();

    /*3.write the byte into stream one by one*/
    size_t Counter = 0;
    while(Counter < ByteWrite)
        _Stream.push_back(data[Counter++]);       
    
    /* 4.add the ByteWrite to TotalWritten */
    _TotalWritten += ByteWrite;

    return ByteWrite;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string PeekStr = "";    // result

    /*1.get how many bytes has not been read*/
    size_t Remainder =  this->buffer_size();

    /*2.the number of byte can be peeked is the smaller value of Remainder and len*/
    size_t BytePeek =  len >= Remainder ? Remainder : len; 

    /*3.copy Byte to PeekStr while _ReadPtr do not move*/
    size_t Counter = 0;

    while(Counter < BytePeek)
        PeekStr.push_back(_Stream[Counter++]);      

    return PeekStr;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 

    /*1.get how many bytes has not been read*/
    size_t Remainder =  this->buffer_size();

    /*2.the number of byte can be popped is the smaller value of Remainder and len*/
    size_t ByteRemoved =  len >= Remainder ? Remainder : len; 

    /*3.modify the _ReadPtr to the appropriate position*/
    size_t Counter = 0;
    while(Counter++ < ByteRemoved)
        _Stream.pop_front();
    
    /*4.add the ByteRemoved to _TotalRead*/
    _TotalRead += ByteRemoved;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    /*combination of peek and pop*/
    /*1.peek*/
    string PeekStr = peek_output(len);

    /*2.pop out the bytes from stream*/ 
    pop_output(len);
    return PeekStr;
}

void ByteStream::end_input() {
    _endin = true;                  // set the end of input flag
}

bool ByteStream::input_ended() const { 
    return _endin == true;          // judge the _endin flag
}

size_t ByteStream::buffer_size() const { 
    return _Stream.size();
}

bool ByteStream::buffer_empty() const { 
    return _Stream.empty(); 
}

bool ByteStream::eof() const { 
    return input_ended() && buffer_empty();     // if the input has ended and the buffer has nothing to be read
    // that is the end of file 
}

size_t ByteStream::bytes_written() const { 
    return _TotalWritten; 
}

size_t ByteStream::bytes_read() const { 
    return _TotalRead; 
}

size_t ByteStream::remaining_capacity() const { 
    return _Capacity - _Stream.size();
}
