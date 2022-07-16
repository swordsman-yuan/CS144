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

size_t TCPConnection::time_since_last_segment_received() const { 
    return this->_LastReceivedTimer.getTime();
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    this->_LastReceivedTimer.resetTimer();      // reset the timer 
    this->_LastReceivedTimer.startTimer();      // restart the timer to record the time passed

    /* 0.1 special case handler, if the segment's RST flag has been set */
    if(seg.header().rst == true)
    {
        /* set the _IsActive flag as false to indicate instant death */
        this->_IsActive = false;

        /* set the error status of both in-bound and out-bound byte stream */
        this->_sender.stream_in().set_error();
        this->_receiver.stream_out().set_error();
        return;
    }

    /* 0.2 special case handler : respond to keep-alive segment */
    if  ( 
            this->_receiver.ackno().has_value() && 
            this->_receiver.ackno() - 1 = seg.header().seqno && 
            seg.length_in_sequence_space() == 0         // feature of keep-alive segment
        )
        {
            this->_sender.send_empty_segment();         // construct an empty segment and send out
            while(this->_sender.segments_out().empty() == false)
            {
                /* the payload is empty, but ack, ackno & window size has to be set correctly */
                TCPSengment Front = this->_sender.segments_out().front();
                this->_sender.segments_out().pop();

                if(this->_receiver.ackno().has_value() == true)
                {
                    Front.header().ack = true;                                                                      // set the ack flag
                    Front.header().ackno = this->_receiver.ackno().value();                                         // set the seqno if existing
                    Front.header().win = this->_receiver.window_size() > std::numeric_limits<uint16_t>::max() ?     // set window size
                                            std::numeric_limits<uint16_t>::max() : this->_receiver.window_size();
                }
                this->segments_out().push(Front);
            }
            return;
        }

    /* 1.extract the segment needed by receiver */
    /* the sender will extract ISN, seqno, payload, FIN */
    this->_receiver.segment_received(seg);                  // ackno and window_size will be updated in receiver

    /* 2.extract the segment needed by sender */
    if(seg.header().ack == true)
    {
        /* 2.1 extract ackno and window_size if ack flag is set */
        WrappingInt32 Ackno = seg.header().ackno;
        uint16_t WindowSize = seg.header().win;

        /* 2.2 invoke the sender's ack_received function */
        this->_sender.ack_received(Ackno, WindowSize);          // the sender may try to fill the window after invoking ack_received

        /* 3 scan the sender's _segments_out queue and send out new segments */
        /* and push all the segments into TCPConnection's _segment_out */
        /* sender has filled in seqno(ISN), payload, SYN, FIN if needed */
        /* TCPConnection has to fill ACK, ackno and window_size */
        while(this->_sender.segments_out().empty() == false)
        {
            TCPSegment Front = this->_sender.segments_out().front();
            this->_sender.segments_out().pop();

            /* set the segments's field if necessary */
            if(this->_receiver.ackno().has_value() == true)
            {
                Front.header().ack = true;                                                                      // set the ack flag
                Front.header().ackno = this->_receiver.ackno().value();                                         // set the seqno if existing
                Front.header().win = this->_receiver.window_size() > std::numeric_limits<uint16_t>::max() ?     // set window size
                                        std::numeric_limits<uint16_t>::max() : this->_receiver.window_size();
            }
            this->segments_out().push(Front);
        }
    }
}

bool TCPConnection::active() const { 
    return this->_IsActive; 
}

size_t TCPConnection::write(const string &data) {
    /* write data to byte stream directly */
    return this->_sender.stream_in().write(data);       
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    /* 1.tell the sender about the passage of time */
    this->_sender.tick(ms_since_last_tick);     // may trigger the exponential backoff algorithm

    /* 2. check the retransmission times  */
}

void TCPConnection::end_input_stream() {
    /* set the _endin flag as true */
    this->_sender.stream_in().end_input();      
}

// Initiate a connection by sending a SYN segment
void TCPConnection::connect() {
    /* 1.invoke the fill_window method of sender directly */
    /* at first, the sender will send only one TCPSegment whose SYN flag has been set */
    this->_sender.fill_window();

    /* 2.take out the TCPSegment and send out */
    TCPSegment Front = this->_sender.segments_out().front();
    this->_sender.segments_out().pop();     // pop out the segment cause it has been sent out

    /* the ackno and window size is meaningless, ignore them, send directly */
    this->segments_out().push(Front);

    /* 3.set the _IsActive flag as true */
    this->_IsActive = true;
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
