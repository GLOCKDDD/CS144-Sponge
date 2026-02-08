#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

ByteStream::ByteStream(const size_t capacity)
    : _buffer()                 // 1. 初始化 buffer (调用默认构造)
    , _capacity(capacity)       // 2. 初始化容量 (使用参数)
    , _write_count(0)           // 3. 显式初始化为 0
    , _read_count(0)            // 4. 显式初始化为 0
    , _is_end_input(false)      // 5. 显式初始化为 false
{}

size_t ByteStream::write(const std::string &data) 
{
    size_t cnt = 0;
    for(auto a : data)
    {
        if(1 + _buffer.size() <= _capacity)
        {
            //也许可以改成emplace_back
            _buffer.push_back(a);
            cnt++;
            _write_count++;
        }
        else break;
    }
    return cnt;
}

//! \param[in] len bytes will be copied from the output side of the buffer
std::string ByteStream::peek_output(const size_t len) const 
{
    std::string temp;
    size_t min_len = std::min(len,_buffer.size());
    for(size_t i = 0;i<min_len;i++)
    {
        temp += _buffer[i];
    }
    return temp;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len)
{
    size_t min_len = std::min(len,_buffer.size());
    for(size_t i = 0;i<min_len;i++)
    {
        _buffer.pop_front();
        _read_count++;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) 
{
    std::string temp = this->peek_output(len);
    this->pop_output(len);
    return temp;
}

void ByteStream::end_input()
{
    _is_end_input = true;
}

bool ByteStream::input_ended() const { return _is_end_input; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && _is_end_input; }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
