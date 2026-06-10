#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}
//构造arpframe
void NetworkInterface::send_frame(const EthernetAddress &dst, const uint16_t type, BufferList payload) {
    EthernetFrame frame;
    frame.header().dst = dst;
    frame.header().src = _ethernet_address;
    frame.header().type = type;
    frame.payload() = move(payload);
    _frames_out.push(move(frame));
}
//更新并维护映射
//更新ip->mac
//删除arprequest等待
//检查等待队列并发送
void NetworkInterface::learn_mapping(const uint32_t ip_address, const EthernetAddress &ethernet_address) {
    _arp_cache[ip_address] = {ethernet_address, ARP_CACHE_TIMEOUT};
    _pending_arp_requests.erase(ip_address);

    auto waiting = _waiting_datagrams.find(ip_address);
    if (waiting == _waiting_datagrams.end()) {
        return;
    }

    for (const auto &datagram : waiting->second) {
        send_frame(ethernet_address, EthernetHeader::TYPE_IPv4, datagram.serialize());
    }
    _waiting_datagrams.erase(waiting);
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    const auto arp_entry = _arp_cache.find(next_hop_ip);
    if (arp_entry != _arp_cache.end()) {//找到就发送
        send_frame(arp_entry->second.ethernet_address, EthernetHeader::TYPE_IPv4, dgram.serialize());
        return;
    }

    _waiting_datagrams[next_hop_ip].push_back(dgram);
    //ip->mac映射表中没有的情况下，最近发送过arprequest，就不发
    if (_pending_arp_requests.find(next_hop_ip) != _pending_arp_requests.end()) {
        return;
    }

    ARPMessage request;
    request.opcode = ARPMessage::OPCODE_REQUEST;
    request.sender_ethernet_address = _ethernet_address;
    request.sender_ip_address = _ip_address.ipv4_numeric();
    request.target_ip_address = next_hop_ip;
    //发送arprequest
    send_frame(ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, request.serialize());
    _pending_arp_requests[next_hop_ip] = ARP_REQUEST_TIMEOUT;
}
//接收帧的函数
//如果是ipv4就接收
//如果是arprequest，先学习再回复
//如果是arpreply，学习
//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address and frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }//过滤掉不是发送给自己的
    //接收
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;//
        if (datagram.parse(frame.payload()) == ParseResult::NoError) {
            return datagram;
        }
        return nullopt;
    }

    if (frame.header().type != EthernetHeader::TYPE_ARP) {
        return nullopt;
    }

    ARPMessage message;
    if (message.parse(frame.payload()) != ParseResult::NoError) {
        return nullopt;
    }

    learn_mapping(message.sender_ip_address, message.sender_ethernet_address);

    if (message.opcode == ARPMessage::OPCODE_REQUEST and message.target_ip_address == _ip_address.ipv4_numeric()) {
        ARPMessage reply;
        reply.opcode = ARPMessage::OPCODE_REPLY;
        reply.sender_ethernet_address = _ethernet_address;
        reply.sender_ip_address = _ip_address.ipv4_numeric();
        reply.target_ethernet_address = message.sender_ethernet_address;
        reply.target_ip_address = message.sender_ip_address;

        send_frame(message.sender_ethernet_address, EthernetHeader::TYPE_ARP, reply.serialize());
    }

    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = _arp_cache.begin(); it != _arp_cache.end();) {
        if (it->second.time_to_live <= ms_since_last_tick) {
            it = _arp_cache.erase(it);
        } else {
            it->second.time_to_live -= ms_since_last_tick;
            ++it;
        }
    }

    for (auto it = _pending_arp_requests.begin(); it != _pending_arp_requests.end();) {
        if (it->second <= ms_since_last_tick) {
            it = _pending_arp_requests.erase(it);
        } else {
            it->second -= ms_since_last_tick;
            ++it;
        }
    }
}
