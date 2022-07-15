#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return this->_sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { 
    return this->_sender.bytes_in_flight(); 
}

size_t TCPConnection::unassembled_bytes() const { 
    return this->_receiver.unassembled_bytes(); 
}

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::segment_received(const TCPSegment &seg) { DUMMY_CODE(seg); }

bool TCPConnection::active() const { return {}; }

size_t TCPConnection::write(const string &data) {
    /* write data to byte stream directly */
    return this->_sender.stream_in().write(data);       
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

void TCPConnection::end_input_stream() {
    /* set the _endin flag as true */
    this->_sender.stream_in().end_input();      
}

// Initiate a connection by sending a SYN segment
void TCPConnection::connect() {
    /* 1.invoke the fill_window method of sender directly */
    /* at first, the sender will send a TCPSegment whose SYN flag has been set */
    this->_sender.fill_window();

    /* 2.take out the TCPSegment and send out */
    TCPSegment Front = this->_sender.segments_out().front();
    this->_sender.segments_out().pop();     // pop out the segment cause it has been sent out

    /* the ackno and window size is meaningless, ignore them, send directly */
    this->segments_out().push(Front);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
