#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

/* helper class : retransmission timer  */
class Timer{
private:
    size_t _ElapsedTime{0};
    bool _IsStarted{false};

public:
    /* a set of functions to access & manipulate timer */
    /* implemented by zheyuan */
    Timer(){}
    size_t getTime() const { return this->_ElapsedTime ; }
    void startTimer(){ this->_IsStarted = true ; }
    bool isStarted() const { return this->_IsStarted ;  }
    void resetTimer(){ this->_IsStarted = false ; this->_ElapsedTime = 0 ; }
    void timeElapsedBy(size_t Increment){ if(this->_IsStarted) this->_ElapsedTime += Increment ; } 
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    /* private member added by zheyuan */
    bool _IsSYN{false};                             // is SYN flag has been sent out
    bool _IsFIN{false};                             // is FIN flag has been sent out
    uint16_t _WindowSize{1};                        // newest window size sent by receiver
    uint16_t _RemainingSpace{1};                    // remaining space in receiver's window
    unsigned int _RetransmissionTimes{0};           // Number of consecutive retransmissions that have occurred in a row
    unsigned int _RetransmissionTimeout;            // RTO
    size_t _ByteInFlight{0};              
    Timer _RetransmissionTimer{};                   // Timer
    std::queue<TCPSegment> _NotAcknowledged{};      // TCPSegment which has been sent out but has not been acknowledged
    /* private member added by zheyuan */

  public:
    /* helper function added by zheyuan */
    uint16_t getRemainingSpace(){ return this->_RemainingSpace; }
    bool isFINSent(){ return this->_IsFIN; }
    /* helper function added by zheyuan */

    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
