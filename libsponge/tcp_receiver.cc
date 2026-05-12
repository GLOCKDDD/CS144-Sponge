#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

//同步序列号，将报文段放入重组流中
void TCPReceiver::segment_received(const TCPSegment &seg) 
{
    const TCPHeader& header = seg.header();
    //接收syn
    if(header.syn)
    {
        if(_isn.has_value()) {}
        else {_isn = header.seqno;}
    }
    //还未接收过syn，丢弃
    if(!_isn.has_value()) return;
    else 
    {
        //使用下一个期望接收到的绝对序列号作为checkpoint
        uint64_t checkpoint = _reassembler.stream_out().bytes_written() + 1;
        uint64_t abseqno = unwrap(header.seqno,*_isn,checkpoint);//绝对序列值
        if(!abseqno&&!header.syn) return;//序列号为0但没有syn标志
        uint64_t stream_idx = abseqno - 1 + (header.syn?1:0);//reassembler中payload的索引，需要考虑syn的情况，防止下溢
        _reassembler.push_substring(seg.payload().copy(),stream_idx,header.fin);//写入
    }
    
}

std::optional<WrappingInt32> TCPReceiver::ackno() const 
{ 
    if(!_isn.has_value()) return std::nullopt;

    uint64_t abs_ack = 1 + _reassembler.stream_out().bytes_written();

    if (_reassembler.stream_out().input_ended()) //fin
    {
        abs_ack += 1;
    }

    return wrap(abs_ack, *_isn);
    
}

size_t TCPReceiver::window_size() const 
{ 
    return _capacity - _reassembler.stream_out().buffer_size(); 
}
