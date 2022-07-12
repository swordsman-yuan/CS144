#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    ,_RetransmissionTimeout(retx_timeout){}

uint64_t TCPSender::bytes_in_flight() const { return _ByteInFlight; }

void TCPSender::fill_window() {
    /* fill the window according to _RemainingSpace */
    /* send out segment as much as possioble if there are */
    /* 1.bytes to be read from byte_stream */
    /* 2.there exists space in window */
    while(this->_RemainingSpace != 0 && this->stream_in().buffer_empty())
    {
        TCPSegment Segment;
        
        /* 1.build header, set the seqno*/
        TCPHeader Header;
        Header.seqno = this->next_seqno();

        /* 2.build the payload */
        /* 2.1 calculate the length should be read from byte stream */
        size_t ReadLength = this->_RemainingSpace <= TCPConfig::MAX_PAYLOAD_SIZE ?
                            this->_RemainingSpace : TCPConfig::MAX_PAYLOAD_SIZE ; 

        /* 2.2 the length of Data may be less than ReadLength */
        std::string Data = this->stream_in().read(ReadLength);
        Buffer DataBuffer(Data);                                    // pack the data into a buffer

        /* if byte stream has reached eof */
        if(this->stream_in().eof())
            Header.fin = true;

        /* if this is the first segment been sent out */
        if(this->_IsSYN == false)
        {
            Header.syn = true;
            this->_IsSYN = true;                    // the SYN can be sent only once
        }
            
        
        /* pack the Header and Data into a segment */
        Segment.header() = Header;
        Segment.payload() = DataBuffer;

        /* 3.send out the segment immediately */
        this->segments_out().push(Segment);
        this->_NotAcknowledged.push(Segment);
        if(this->Timer.isStarted() == false)        // if the timer has not started, start it immediately
            this->Timer.startTimer();

        /* 3. modify the status of sender if necessary */
        this->_RemainingSpace = this->_RemainingSpace - Data.size();        // update remaining space
        size_t OccupiedSpace = Segment.length_in_sequence_space();      
        this->_next_seqno += OccupiedSpace;                                 // update _next_seqno
        this->_ByteInFlight += OccupiedSpace;                               // update _ByteInFlight
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {

    /* flag indicating whether segments has been acknowledged by  receiver */
    bool PopFlag = false;   

    /* 1.calculate the absolute seqno of ackno */
    /* using _next_seqno as checkpoint */
    uint64_t AckAbsSeqno = unwrap(ackno, this->_isn, this->_next_seqno);

    /* 2.scan the _NotAcknowledge queue  */
    while(!this->_NotAcknowledged.empty())
    {
        TCPSegment Front = this->_NotAcknowledged.front();
        uint64_t AbsSeqno = unwrap(Front.header().seqno, this->_isn, this->_next_seqno);    // calculate the abs index of segment
        size_t OccupiedSpace = Front.length_in_sequence_space();                            // the number of seqno occupied by segment

        /* the segment has been fully acknowledged by receiver */
        if(AbsSeqno + OccupiedSpace <= AckAbsSeqno)
        {
            this->_NotAcknowledged.pop();   // pop out the segment
            this->_ByteInFlight = this->_ByteInFlight - OccupiedSpace;
            PopFlag = true;
        }
        else break;
    }

    /* 3.reset some status in sender */
    if(PopFlag)
    {
        this->_RetransmissionTimes = 0;                                     // reset retransmission times
        this->_RetransmissionTimeout = _initial_retransmission_timeout;     // reset RTO
        this->Timer.resetTimer();                                           // reset timer
    }

    /* 4.start the timer again if _NotAcknowledged is not empty */
    if(!this->_NotAcknowledged.empty())
        this->Timer.startTimer();

    /* 5.update the window size according to window_size conveyed by receiver */
    this->_WindowSize = window_size;

    /* even if window_size is 0, treat it as 1, which is to prevent dead lock  */
    uint16_t ModifiedWindowSize = window_size >= 1 : window_size : 1;           
    this->_RemainingSpace = ModifiedWindowSize <= this->_ByteInFlight ? 0 : (ModifiedWindowSize - this->_ByteInFlight);
    
    /* fill in the window if new space has opened up */
    if(this->_RemainingSpace > 0)
        fill_window();
 }

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    /* 1.add elapsed time to timer */
    this->Timer.timeElapsedBy(ms_since_last_tick);

    /* 2.if timer has expired */
    if(this->Timer.getTime() > this->_RetransmissionTimeout)
    {
        /* retransmit the earliest unacknowledged segment */
        TCPSegment Front = this->_NotAcknowledged.front();
        this->_segments_out.push(Front);

        /* if the newest window size is not zero */
        if(this->_WindowSize > 0)
        {
            this->_RetransmissionTimes ++;          // increment the retransmission times
            this->_RetransmissionTimeout << 1;      // double the RTO : exponential backoff algorithm
        }
    }

    /* 3.reset the timer and restart it */
    this->Timer.resetTimer();
    this->Timer.startTimer();
 }

unsigned int TCPSender::consecutive_retransmissions() const { return this->_RetransmissionTimes; }

void TCPSender::send_empty_segment() {
    TCPSegment Segment;

    /* 1.build the header, set the seqno appropriately */
    TCPHeader Header;
    Header.seqno = this->next_seqno();

    /* 2.build the payload, which is empty */
    Buffer Data("");

    /* 3.pack the header and data into segment */
    Segment.header() = Header;
    Segment.payload() = Data;
    this->segments_out().push(Segment);     // send out the segment
}
