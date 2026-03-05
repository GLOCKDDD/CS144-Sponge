#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.


using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _timeout{retx_timeout}
    , _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }


void TCPSender::fill_window() 
{

    size_t window_limit = _current_windows_size == 0 ? 1 : _current_windows_size;

    //当途中没有数据且window_limit == 1时触发零探测
    while(window_limit > _bytes_in_flight&&!_fin_sent)
    {
        TCPSegment seg;

        //因为limit最小为一，顺便解决了syn报文段的byte问题
        if(_next_seqno == 0) seg.header().syn = true;

        size_t remaining_window = window_limit - _bytes_in_flight - seg.length_in_sequence_space();

        //将abs_seq转换成相对seq
        seg.header().seqno = wrap(_next_seqno,_isn);
        
        seg.payload() = Buffer(std::move(_stream.read(std::min(remaining_window,TCPConfig::MAX_PAYLOAD_SIZE))));

        //当上层数据写入并读取完毕并且还有剩余空间时设置fin字段
        if(_stream.eof()&&remaining_window > seg.payload().size()) 
        {
            seg.header().fin = true;
            _fin_sent = true;
        }

        size_t len = seg.length_in_sequence_space();

        //当没有数据可以发送时结束发送，
        if(!len) break;

        //
        _next_seqno += len;
        _bytes_in_flight += len;

        _segments_out.push(seg);
        if(_segments_in_flight.empty())
        {
            _timer_is_open = true;
            _timer =  0;
        }
        _segments_in_flight.push(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) 
{
    //更新接收窗口，重传，在途

    uint64_t abs_ackno = unwrap(ackno,_isn,_next_seqno);

    if(abs_ackno > _next_seqno) return;

    //更新接收窗口
    _current_windows_size = window_size;

    bool has_seg_acked = false;

    //更新在途队列
    while(!_segments_in_flight.empty())
    {
        const TCPSegment& seg = _segments_in_flight.front();

        uint64_t abs_seqno = unwrap(seg.header().seqno,_isn,_next_seqno);

        if(abs_seqno + seg.length_in_sequence_space() <= abs_ackno)
        {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_in_flight.pop();
            has_seg_acked = true;
        }
        else break;

    }

    //重置定时器，cnt，重传时间
    if (has_seg_acked) {
        _consecutive_retransmissions_cnt = 0;

        _timeout = _initial_retransmission_timeout;

        _timer = 0;

        if (_segments_in_flight.empty()) {
            _timer_is_open = false;
        } else
            _timer_is_open = true;
    }

    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) 
{
    //更新计时器，判断是否重传，翻倍重传时间

    if(!_timer_is_open) return;

    _timer += ms_since_last_tick;

    if(_timer >= _timeout)
    {
        _segments_out.push(_segments_in_flight.front());

        _timer = 0;

        //零探测并且上一个报文不是syn报文段
        if(_current_windows_size == 0 && !_segments_in_flight.front().header().syn) return;

        _consecutive_retransmissions_cnt += 1;

        _timeout = _timeout*2;

    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_cnt; }

void TCPSender::send_empty_segment() 
{
    TCPSegment seg;

    seg.header().seqno = wrap(_next_seqno,_isn);

    _segments_out.push(seg);
}
