#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    /*1.set the initial sequence number if available*/
    /*check whether the SYN flag is true*/
    WrappingInt32 Seqno = seg.header().seqno; // extract the seqno
    if(seg.header().syn)                      // if the SYN flag is true
    {
        this->_ISN = Seqno;
        this->_IsISN = true;      // now the ISN is valid
    }

    /*if the ISN has not been set, the segment is invalid*/
    if(this->_IsISN == false)
        return;
        
    /*2.push any data or end-of-stream marker to the StreamReassembler*/
    /*2.1 extract the payload out of TCPSegment*/
    std::string Data = seg.payload().copy();

    /*2.2 calculate the index of newly-arrived data*/
    /*for the TCPSegment whose syn flag is true, 
    the seqno of first byte of data should be seqno + 1*/
    WrappingInt32 ModifiedSeqno = seg.header().syn ? operator+(Seqno, 1) : Seqno;

    /*2.3 calculate checkpoint : it should be the the index of the last reassembled byte*/
    /*when reassembler is waiting for its first byte, the ckpt is 0*/
    uint64_t ModifiedCheckPoint = this->_reassembler.getNextIndex() == 0 ? 0 : this->_reassembler.getNextIndex() - 1;

    /*2.4 calculate the stream index */
    // minus 1 to convert AbsSeqno to stream indice
    uint64_t StreamIndice = unwrap(ModifiedSeqno, this->_ISN, ModifiedCheckPoint) - 1; 

    /*2.5 write the newly-arrived string to stream reassembler*/
    this->_reassembler.push_substring(Data, StreamIndice, seg.header().fin); 

    /*QUESTION: When to set the _IsISN flag to false?*/
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    /*0. special case handler: the ISN has not been set*/
    if(this->_IsISN == false)
        return std::nullopt;

    /*1. inquire stream reassembler for _NextIndex, plus 1 to get AbsoluteSeqNo*/
    uint64_t NextIndex = this->_reassembler.getNextIndex() + 1;

    /*2. convert the AbsoluteSeqNo to SeqNo and return it back*/
    return wrap(NextIndex, this->_ISN); 
}

size_t TCPReceiver::window_size() const { 
    return this->stream_out().remaining_capacity(); 
}
