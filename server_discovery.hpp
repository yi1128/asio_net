#pragma once

#include <array>
#include <functional>
#include <string>

#include "asio.hpp"
#include "detail/log.h"

namespace asio_net {

/**
 * message format:
 * prefix + '\n' + name + '\n' + message
 * i.e: "discovery\nname\nmessage
 */
namespace server_discovery {

const char* addr_default = "239.255.0.1";
const short port_default = 30001;

class receiver {
  using service_found_handle_t = std::function<void(std::string name, std::string message)>;

 public:
  receiver(asio::io_context& io_context, service_found_handle_t handle, const char* addr = addr_default, short port = port_default)
      : socket_(io_context), service_found_handle_(std::move(handle)) {
    // create the socket so that multiple may be bound to the same address.
    asio::ip::udp::endpoint listen_endpoint(asio::ip::make_address("0.0.0.0"), port);
    socket_.open(listen_endpoint.protocol());
    socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    socket_.bind(listen_endpoint);

    // join the multicast group.
    socket_.set_option(asio::ip::multicast::join_group(asio::ip::make_address(addr)));

    do_receive();
  }

 private:
  void do_receive() {
    socket_.async_receive_from(asio::buffer(data_), sender_endpoint_, [this](std::error_code ec, std::size_t length) {
      if (!ec) {
        std::vector<std::string> msgs;
        {
          msgs.reserve(3);
          auto begin = data_.data();
          auto end = data_.data() + length;
          decltype(begin) it;
          int count = 0;
          while ((it = std::find(begin, end, '\n')) != end) {
            msgs.emplace_back(begin, it);
            begin = it + 1;
            if (++count > 2) break;
          }
          msgs.emplace_back(begin, it);
        }
        if (msgs.size() == 3 && msgs[0] == "discovery") {
          service_found_handle_(std::move(msgs[1]), std::move(msgs[2]));
        }
        do_receive();
      } else {
        asio_net_LOGE("server_discovery: receive: err: %s", ec.message().c_str());
      }
    });
  }

  asio::ip::udp::socket socket_;
  asio::ip::udp::endpoint sender_endpoint_;
  std::array<char, 1024> data_;
  service_found_handle_t service_found_handle_;
};

class sender {
 public:
  sender(asio::io_context& io_context, const std::string& service_name, const std::string& message, uint send_period_sec = 1,
         const char* addr = addr_default, short port = port_default)
      : endpoint_(asio::ip::make_address(addr), port),
        socket_(io_context, endpoint_.protocol()),
        timer_(io_context),
        send_period_sec_(send_period_sec),
        message_("discovery\n" + service_name + '\n' + message) {
    do_send();
  }

 private:
  void do_send() {
    socket_.async_send_to(asio::buffer(message_), endpoint_, [this](std::error_code ec, std::size_t /*length*/) {
      if (!ec) {
        do_send_next();
      } else {
        asio_net_LOGE("server_discovery: sender: err: %s", ec.message().c_str());
      }
    });
  }

  void do_send_next() {
    timer_.expires_after(std::chrono::seconds(send_period_sec_));
    timer_.async_wait([this](std::error_code ec) {
      if (!ec) do_send();
    });
  }

 private:
  asio::ip::udp::endpoint endpoint_;
  asio::ip::udp::socket socket_;
  asio::steady_timer timer_;
  uint send_period_sec_;
  std::string message_;
};

}  // namespace server_discovery
}  // namespace asio_net
