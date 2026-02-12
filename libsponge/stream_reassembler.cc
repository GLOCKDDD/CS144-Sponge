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
    // 1. 【记录 EOF】如果这是最后一块，记录下终点位置
    if (eof) {
        _eof_index = index + data.size();
    }

    // 2. 【准备工作】获取当前流的状态
    // expect: 下一个渴望接收的绝对索引
    size_t expect = _output.bytes_written(); 
    // limit: 也就是 first_unavailable，超过这个位置的字节直接丢弃
    // 注意：容量是针对 "未读出的数据" + "未组装的数据" 的总和
    size_t limit = _output.bytes_read() + _capacity;

    // 3. 【剪裁数据 (Trimming)】
    // 我们用一个新变量 nindex 和 ndata 来代表处理后的数据
    size_t nindex = index;
    std::string ndata = data;

    // 情况 A: 数据在 expect 之前 (旧数据)
    if (nindex < expect) {
        // 如果整个数据都在 expect 之前，直接忽略
        if (nindex + ndata.size() <= expect) {
            goto check_done; // 跳到底部检查是否结束
        }
        // 切掉前面重叠的部分
        ndata = ndata.substr(expect - nindex);
        nindex = expect;
    }

    // 情况 B: 数据超出了容量限制 (尾部溢出)
    if (nindex + ndata.size() > limit) {
        // 如果连开头都超出了，直接忽略
        if (nindex >= limit) {
            goto check_done;
        }
        // 切掉后面超出的部分
        ndata = ndata.substr(0, limit - nindex);
    }

    // 如果剪裁完数据空了，直接结束
    if (ndata.empty()) {
        goto check_done;
    }

    // 4. 【合并逻辑 (Merging)】 核心难点！
    // 现在的 nindex 和 ndata 是干净的、符合容量的。
    // 我们要把它塞进 map 里，但要先看看有没有撞到 map 里已有的块。
    {
        // 算出新数据的结尾
        size_t nend = nindex + ndata.size();

        // 查找第一个可能重叠的块 (key >= nindex)
        auto it = _buffer.lower_bound(nindex);

        // 【向左看】：检查前一个块是否跟我们重叠
        // 比如 map: [0, 10], 我们插入 [5, 15]。lower_bound(5) 会指向 end() 或者后面的块
        // 我们必须回头看一眼 [0, 10]
        if (it != _buffer.begin()) {
            auto prev = std::prev(it);
            // 如果前一个块的结尾 > 我们的开头，说明重叠了
            if (prev->first + prev->second.size() > nindex) {
                it = prev; // 把迭代器移回去，从前一个块开始处理
            }
        }

        // 【循环吞噬】：只要 it 指向的块跟我们需要的位置有重叠，就合并
        while (it != _buffer.end()) {
            size_t cur_index = it->first;
            size_t cur_end = cur_index + it->second.size();

            // 如果当前块的起点都在我们终点之后了，说明后面没重叠了，退出循环
            if (cur_index >= nend) {
                break;
            }

            // --- 开始合并 ---
            
            // 1. 扩展左边界：如果旧块开始得更早，更新 nindex 和 ndata
            if (cur_index < nindex) {
                ndata = it->second.substr(0, nindex - cur_index) + ndata;
                nindex = cur_index;
            }

            // 2. 扩展右边界：如果旧块结束得更晚，更新 ndata 和 nend
            if (cur_end > nend) {
                ndata = ndata + it->second.substr(nend - cur_index);
                nend = cur_index + it->second.size();
            }

            // 3. 删除旧块 (因为它已经被融合进 ndata 了)
            _unassembled_bytes -= it->second.size(); // 从计数器减去
            it = _buffer.erase(it); // erase 返回下一个元素的迭代器
        }

        // 【插入】：把融合后的超级大块放进 map
        _buffer[nindex] = ndata;
        _unassembled_bytes += ndata.size();
    }

    // 5. 【写入流 (Writing)】
    // 检查 map 的第一块是否正好接上了 expect
    while (!_buffer.empty()) {
        auto it = _buffer.begin();
        
        // 如果第一块的索引 != 期望索引，说明断片了，没法写
        if (it->first != _output.bytes_written()) {
            break;
        }

        // 写入流
        _output.write(it->second);
        
        // 从缓存移除
        _unassembled_bytes -= it->second.size();
        _buffer.erase(it);
    }

check_done:
    // 6. 【检查是否大结局】
    // 只有当“已写入的字节数”等于我们之前记录的“EOF位置”时，才真正关闭
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

