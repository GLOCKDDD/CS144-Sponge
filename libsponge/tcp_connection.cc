#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.


using namespace std;

void TCPConnection::send_sender_segments()
{
    std::queue<TCPSegment>& q = _sender.segments_out();

    while(!q.empty())
    {
        TCPSegment seg = std::move(q.front());

        q.pop();

        if(_receiver.ackno().has_value())
        {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }

        _segments_out.push(std::move(seg));
    }
}

size_t TCPConnection::remaining_outbound_capacity() const { return {_sender.stream_in().remaining_capacity()}; }

size_t TCPConnection::bytes_in_flight() const { return {_sender.bytes_in_flight()}; }

size_t TCPConnection::unassembled_bytes() const { return {_receiver.unassembled_bytes()}; }

size_t TCPConnection::time_since_last_segment_received() const { return {_last_segment_received_time}; }


void TCPConnection::segment_received(const TCPSegment &seg) 
{
    _last_segment_received_time = 0;

    //收到强制中断连接的RST报文段
    if(seg.header().rst)
    {
        _rst_received = true;
        //防御性编程，不用延迟关闭
        _linger_after_streams_finish = false;

        //连接异常，防止还在队列中的数据包发送出去
        while(!_segments_out.empty())
        {
            _segments_out.pop();
        }
        return;
    }

    _receiver.segment_received(seg);

    if(seg.header().ack)
    {
        _sender.ack_received(seg.header().ackno,seg.header().win);
    }

    //接收后返回ack或者窗口变长，可以发送报文段
    _sender.fill_window();

    //判断是否需要发送纯ack报文段
    if(_sender.segments_out().empty()&&_receiver.ackno().has_value()&&seg.length_in_sequence_space())
    {
        _sender.send_empty_segment();
    }

    send_sender_segments();

    if(seg.header().fin)
    {
        //在已经发送过fin的情况下收到fin报文段，需要延迟关闭
        if(_sender.stream_in().eof())   _linger_after_streams_finish = true;
        else _linger_after_streams_finish = false;
    }
    
}

bool TCPConnection::active() const
{
    if(_rst_received) return false;
    //连接的正常关闭：接收和发送完全，并不延迟
    else if(_receiver.stream_out().input_ended()//收到fin报文段
    &&_sender.stream_in().eof()//应用层停止输入并读完
    &&_sender.bytes_in_flight() == 0
    &&!_linger_after_streams_finish
    )
    {
        return false;
    }
     return true; 
}

size_t TCPConnection::write(const string &data) 
{

    size_t result = _sender.stream_in().write(data);

    if (result) {

        _sender.fill_window();

        send_sender_segments();
    }

    return result;
}

void TCPConnection::send_rst() 
{
    TCPSegment seg;

    seg.header().rst = true;

    seg.header().seqno = _sender.next_seqno();

    if (_receiver.ackno().has_value()) {
        seg.header().ackno = _receiver.ackno().value();

        seg.header().ack = true;
    }

    _segments_out.push(std::move(seg));

    _linger_after_streams_finish = false;//不需要等待，直接关闭

    //转换状态
    _rst_received = true;

    //清理队列
    while(!_segments_out.empty())
    {
        _segments_out.pop();
    }


    return;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) 
{ 

    _last_segment_received_time += ms_since_last_tick;
    
    //判断是否重发
    _sender.tick(ms_since_last_tick);

    //重传次数过多，发送rst报文
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
    {
        send_rst();
    }

    if (_sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended() &&
        _linger_after_streams_finish) 
    {
        const size_t rt_time_out = _cfg.rt_timeout*10;
        if(_last_segment_received_time >= rt_time_out)
        {
            _linger_after_streams_finish = false;
        }
    }
}

void TCPConnection::end_input_stream() 
{
    //结束输入
    _sender.stream_in().end_input();
    //打包fin报文段
    _sender.fill_window();

    send_sender_segments();
}

void TCPConnection::connect() 
{
    //发送syn报文

    _sender.fill_window();

   send_sender_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
