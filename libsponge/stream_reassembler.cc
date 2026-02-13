#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

StreamReassembler::StreamReassembler(const size_t capacity) : _buffer(),_eof_index(-1),_unassembled_bytes(0),_output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
// 对于unreassemble的函数，被缓存的相同索引的字符只能被计数一次
// 该如何处理eof
// 整个streamreassembler的容量限制是_capacity
// 对于下一个期望接收字符的索引应该调用bytes_written()函数
void StreamReassembler::push_substring(const std::string &data, const size_t index, const bool eof) 
{
    // --- 1. 记录 EOF ---
    if (eof) {
        _eof_index = index + data.size();
    }

    // --- 2. 准备数据 ---
    size_t first_unread = _output.bytes_read();
    size_t capacity_limit = first_unread + _capacity; // 绝对索引限制
    size_t expect = _output.bytes_written();

    size_t new_idx = index;
    std::string new_data = data;

    // --- 3. 剪裁 (Trimming) ---
    // 3.1 左边：切掉旧数据
    if (new_idx < expect) {
        if (new_idx + new_data.size() <= expect) {
            new_data = ""; // 全部是旧的
        } else {
            new_data = new_data.substr(expect - new_idx);
            new_idx = expect;
        }
    }

    // 3.2 右边：切掉超出 Capacity 的数据
    if (new_idx + new_data.size() > capacity_limit) {
        if (new_idx >= capacity_limit) {
            new_data = ""; // 全部超出
        } else {
            new_data = new_data.substr(0, capacity_limit - new_idx);
        }
    }

    // --- 4. 合并与存储 (仅当数据非空时执行) ---
    if (!new_data.empty()) {
        
        // 4.1 查找重叠起点
        auto it = _buffer.lower_bound(new_idx);
        if (it != _buffer.begin()) {
            auto prev = std::prev(it);
            if (prev->first + prev->second.size() > new_idx) {
                it = prev; // 前一个块也重叠了
            }
        }

        size_t new_end = new_idx + new_data.size();

        // 4.2 循环吞噬重叠块
        while (it != _buffer.end()) {
            // 如果当前块完全在后面，没重叠，由于 map 是有序的，后面也不用看了
            if (it->first >= new_end) {
                break;
            }

            // 合并逻辑：扩展 new_data
            if (it->first < new_idx) {
                new_data = it->second.substr(0, new_idx - it->first) + new_data;
                new_idx = it->first;
            }
            if (it->first + it->second.size() > new_end) {
                new_data = new_data + it->second.substr(new_end - it->first);
                new_end = new_idx + new_data.size();
            }

            // 吃掉旧块
            _unassembled_bytes -= it->second.size();
            it = _buffer.erase(it);
        }

        // 4.3 插入新块
        _buffer[new_idx] = new_data;
        _unassembled_bytes += new_data.size();
    }

    // --- 5. 写入流 (统一处理) ---
    // 只要 map 还有数据，且队头就是我们要的数据
    while (!_buffer.empty() && _buffer.begin()->first == _output.bytes_written()) {
        const auto &head = _buffer.begin();
        
        // 写入 ByteStream
        size_t written = _output.write(head->second);
        
        // 这里的逻辑稍微需要注意：
        // ByteStream 也有容量限制，可能写不完（虽然 Lab0 应该不会）
        // 但简单起见，我们假设通过前面的 capacity_limit 剪裁，这里能写完。
        
        _unassembled_bytes -= head->second.size();
        _buffer.erase(head);
    }

    // --- 6. 检查结束 ---
    if (_output.bytes_written() == _eof_index) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    return _unassembled_bytes; 
}

bool StreamReassembler::empty() const { 
    return _unassembled_bytes == 0; 
}

