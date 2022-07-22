#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

/* helper function added by zheyuan */
/* send out all the segments kept by sender */
/* also set the flag correctly kept by receiver */
/* scan the sender's _segments_out queue and send out new segments */
/* and push all the segments into TCPConnection's _segment_out */
/* sender has filled in seqno(ISN), payload, SYN, FIN if needed */
/* TCPConnection has to fill ACK, ackno and window_size, which are kept by receiver, done by sendSegment */
bool TCPConnection::sendSegment(bool RST){
    if(this->_sender.segments_out().empty())
    /* return false if no segments send out */
        return false;

    while(this->_sender.segments_out().empty() == false)
    {
        TCPSegment Front = this->_sender.segments_out().front();
        this->_sender.segments_out().pop();

        /* set the RST flag if needed */
        if(RST == true)
            Front.header().rst = true;

        /* set the field kept by receiver correctly */
        if(this->_receiver.ackno().has_value() == true)
        {
            Front.header().ack = true;                                                                      // set the ack flag
            Front.header().ackno = this->_receiver.ackno().value();                                         // set the seqno if existing
            Front.header().win = this->_receiver.window_size() > std::numeric_limits<uint16_t>::max() ?     // set window size
                                    std::numeric_limits<uint16_t>::max() : this->_receiver.window_size();
        }
        this->segments_out().push(Front);       // send out the segment
    }

    return true;
}

/* helper function added by zheyuan */

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
    /* 0.1 special case handler, if the segment's RST flag has been set */
    if(seg.header().rst == true)
    {
        /* set the _IsActive flag as false to indicate instant death */
        this->_IsActive = false;

        /* set the error status of both in-bound and out-bound byte streams */
        this->_sender.stream_in().set_error();
        this->_receiver.stream_out().set_error();
        return;
    }

    /* 0.2 special case handler : respond to keep-alive segment */
    if  ( 
            this->_receiver.ackno().has_value() && 
            operator-(this->_receiver.ackno().value(), 1) == seg.header().seqno && 
            seg.length_in_sequence_space() == 0         // feature of keep-alive segment
        )
        {
            this->_sender.send_empty_segment();         // construct an empty segment and send out
            this->sendSegment(false);                   // send out all the segments, RST is false
            return;
        }

    /* 1.extract the segment needed by receiver */
    /* the receiver will extract ISN, seqno, payload, FIN */
    this->_receiver.segment_received(seg);                  // ackno and window_size will be updated in receiver
    
    /* if the in-bound byte stream has ended and the out-bound byte stream has not reached its EOF */
    if  (
            this->_receiver.stream_out().input_ended() == true &&
            this->_sender.isFINSent() == false
        )
            this->_linger_after_streams_finish = false;     // set the flag which means we don't need to linger : passive close
    
    /* 2.extract the segment needed by sender */
    if(seg.header().ack == true)
    {
        /* 2.1 extract ackno and window_size if ack flag is set */
        WrappingInt32 Ackno = seg.header().ackno;
        uint16_t WindowSize = seg.header().win;

        /* 2.2 invoke the sender's ack_received function */
        this->_sender.ack_received(Ackno, WindowSize);      // the sender may try to fill the window after invoking ack_received

        /* MAIN CODE OF TCP FSM */
        /* REFER TO TCP TRANSITION DIAGRAM */
        if  (
                this->_CurrentState == MyState::SYN_SENT &&
                this->_receiver.isISNReceived() == true &&                  // SYN has been received
                this->_sender.isNotAcknowledgedEmpty() == true              // ack has been received
            )    
        {
            bool SentFlag = this->sendSegment(false);
            if(SentFlag == false)
            {
                this->_sender.send_empty_segment();                         // force the TCP to complete 3-way handshake 
                this->sendSegment(false);
            }  
            this->_CurrentState = MyState::ESTABLISHED;
        }
        else if (
                    this->_CurrentState == MyState::ESTABLISHED &&          // TCP is now in ESTABLISHED state
                    this->_receiver.stream_out().input_ended() == false     // FIN has not been acked
                )               
        {
            bool SentFlag = this->sendSegment(false);
            if(SentFlag == false && this->_receiver.needAck() == true)      // if arriving segment has data, ack is needed
            {
                this->_sender.send_empty_segment();
                this->sendSegment(false);
            }
        }   
        /* passive close */
        else if (
                    this->_CurrentState == MyState::ESTABLISHED &&          // TCP is now in ESTABLISHED state
                    this->_receiver.stream_out().input_ended() == true      // FIN has been received
                )
                {
                    bool SentFlag = this->sendSegment(false);
                    if(SentFlag == false)
                    {
                        this->_sender.send_empty_segment();                 // force the TCP to ack
                        this->sendSegment(false);
                    }
                    this->_CurrentState = MyState::CLOSE_WAIT;
                }
        else if (
                    this->_CurrentState == MyState::CLOSE_WAIT              // TCP is now in CLOSE_WAIT state
                )
                {
                    bool SentFlag = this->sendSegment(false);
                    if(SentFlag == false)
                    {
                        this->_sender.send_empty_segment();                 // force the TCP to ack
                        this->sendSegment(false);                           // just send ack to reply, wait for POSITIVE close action
                    }    
                }
        else if (
                    this->_CurrentState == MyState::LAST_ACK &&
                    this->_sender.isNotAcknowledgedEmpty() == false         // FIN has not been acked
                )
                {
                    this->_sender.send_empty_segment();                     // just reply an ack
                    this->sendSegment(false);
                }
        else if (
                    this->_CurrentState == MyState::LAST_ACK &&             // TCP is now in LAST_ACK state
                    this->_sender.isNotAcknowledgedEmpty() == true          // FIN has been acked
                )
                {
                    if(this->_linger_after_streams_finish == false)
                        this->_IsActive = false;                            // abort the connection immediately, no lingering
                }
        else if (
                    this->_CurrentState == MyState::FIN_WAIT1 &&            // TCP is now in FIN_WAIT1 state
                    this->_receiver.stream_out().input_ended() == true &&   
                    this->_sender.isNotAcknowledgedEmpty() == true          // FIN and ack has been received
                )
        {
            
            this->_sender.send_empty_segment();                             // send an ack to reply
            this->sendSegment(false);
            this->_CurrentState = MyState::TIME_WAIT;                       // the status transform from FIN_WAIT1 to TIMEWAIT 
            if(this->_linger_after_streams_finish == true)
                this->_LastReceivedTimer.startTimer();
        }
        else if (
                    this->_CurrentState == MyState::FIN_WAIT1 &&            // TCP is now in FIN_WAIT1 state
                    this->_receiver.stream_out().input_ended() == false &&  // FIN has not been received
                    this->_sender.isNotAcknowledgedEmpty() == true          // and all the segments(FIN) has been acked
                )
        {
            this->_CurrentState = MyState::FIN_WAIT2;                       // state transform to FIN_WAIT2
        }
        else if (
                    this->_CurrentState == MyState::FIN_WAIT1 &&            // TCP is now in FIN_WAIT1 state
                    this->_receiver.stream_out().input_ended() == true  &&  // FIN has been received
                    this->_sender.isNotAcknowledgedEmpty() == false         // FIN has not been acked
                )
                {
                    this->_sender.send_empty_segment();                     // send a ack to reply
                    this->sendSegment(false);  
                    this->_CurrentState = MyState::CLOSING;
                }
        else if (
                    this->_CurrentState == MyState::FIN_WAIT2 &&            // TCP is now in FIN_WAIT2 state
                    this->_receiver.stream_out().input_ended() == true      // FIN has been received
                )
        {   
            this->_sender.send_empty_segment();                             // send an ack to reply
            this->sendSegment(false);                                       
            this->_CurrentState = MyState::TIME_WAIT;
            if(this->_linger_after_streams_finish == true)                  // start the timer
                this->_LastReceivedTimer.startTimer();
        }
        else if (
                    this->_CurrentState == MyState::CLOSING &&              // TCP is now in CLOSING state and receive ack
                    this->_sender.isNotAcknowledgedEmpty() == true
                )
                {                                                           // send nothing
                    this->_CurrentState = MyState::TIME_WAIT;               // transform to TIME_WAIT state
                    if(this->_linger_after_streams_finish == true)          // start timer
                        this->_LastReceivedTimer.startTimer();
                }
        else if (
                    this->_CurrentState == MyState::TIME_WAIT
                )
        {   
            this->_sender.send_empty_segment();                             // send an ack to reply
            this->sendSegment(false);                                       // state don't transfer, keep it
            this->_LastReceivedTimer.resetTimer();                          
            this->_LastReceivedTimer.startTimer();                          // restart the timer    
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
    /* the tick method may increment the retransmission times */
    /* also, it may retransmit a segment */
    this->_sender.tick(ms_since_last_tick);

    /* retransmit the segment immediately if segment retransmitted */ 
    this->sendSegment(false);    
        
    /* UNCLEAN SHUTDOWN 1 : EXCEED MAX_RETX_ATTEMPTS */
    /* 2. check whether the retransmission times has exceeded the max retransmission times */
    if(this->_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
    {
        /* send a RST segment to abort the connection */
        /* 2.1 force the sender send a segemnt */
        this->_sender.send_empty_segment();

        /* 2.2 set the RST flag and send the empty segment out */
        this->sendSegment(true);

        /* 2.3 set the error bit of in-bound and out-bound byte stream */
        /* also, _IsActive is set to false */
        this->_sender.stream_in().set_error();
        this->_receiver.stream_out().set_error();
        this->_IsActive = false;
        return;
    }

    /* 3.record the time passed since last segment received */
    this->_LastReceivedTimer.timeElapsedBy(ms_since_last_tick); // if lingering is not needed, the timer will not count
    /* abort the connection if enough time has passed */
    size_t WaitingTime = ((this->_cfg.rt_timeout << 3) + (this->_cfg.rt_timeout << 1));
    if(this->_LastReceivedTimer.getTime() >= WaitingTime)    
    {
        this->_IsActive = false;
        return;
    }
}

void TCPConnection::end_input_stream() {
    /* set the _endin flag as true */
    this->_sender.stream_in().end_input();      

    /* if the TCP is in ESTABLISHED status */
    /* the FIN flag should be sent out */
    if(this->_CurrentState == MyState::ESTABLISHED)
    {
        this->_sender.fill_window();
        this->sendSegment(false);                       // send out the segment with FIN set
        this->_CurrentState = MyState::FIN_WAIT1;       // state tranfer to FIN_WAIT1
    }
    else if(this->_CurrentState == MyState::CLOSE_WAIT)
    {
        this->_sender.fill_window();
        this->sendSegment(false);                       // send out segment with FIN set
        this->_CurrentState = MyState::LAST_ACK;        // state transfer to LAST_ACK
    }
}

// Initiate a connection by sending a SYN segment
void TCPConnection::connect() {
    /* 1.invoke the fill_window method of sender directly */
    /* at first, the sender will send only one TCPSegment whose SYN flag has been set */
    this->_sender.fill_window();

    /* 2.take out the TCPSegment and send out */
    if(this->sendSegment(false) == true)
        /* the TCP now enters active connection status */
        this->_CurrentState = MyState::SYN_SENT;           

    /* 3.set the _IsActive flag as true */
    this->_IsActive = true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer

            /* UNCLEAN SHUTDOWN 2 : dtor is called when TCPConnection is active */
            /* send a segment from sender whose RST flag is true */
            this->_sender.send_empty_segment();
            this->sendSegment(true);                        // send segment with RST flag
            this->_sender.stream_in().set_error();          // set error flag in outbound byte stream
            this->_receiver.stream_out().set_error();       // set error flag in inbound byte stream
            this->_IsActive = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
