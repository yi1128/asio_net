#pragma once

#include <utility>

#include "asio.hpp"
#include "rpc_session_t.hpp"
#include "tcp_server_t.hpp"

namespace asio_net {
namespace detail {

template <socket_type T>
class rpc_server_t : noncopyable {
 public:
  rpc_server_t(asio::io_context& io_context, uint16_t port, rpc_config rpc_config = {})
      : io_context_(io_context), rpc_config_(rpc_config), server_(io_context, port, rpc_config.to_tcp_config()) {
    static_assert(T == detail::socket_type::normal, "");
    server_.on_session = [this](std::weak_ptr<detail::tcp_session_t<T>> ws) {
      auto session = std::make_shared<rpc_session_t<T>>(io_context_, rpc_config_);
      session->init(std::move(ws));
      if (on_session) {
        on_session(session);
      }
    };
  }

#ifdef ASIO_NET_ENABLE_SSL
  rpc_server_t(asio::io_context& io_context, uint16_t port, asio::ssl::context& ssl_context, rpc_config rpc_config = {})
      : io_context_(io_context), rpc_config_(rpc_config), server_(io_context, port, ssl_context, rpc_config.to_tcp_config()) {
    static_assert(T == detail::socket_type::ssl, "");
    server_.on_session = [this](std::weak_ptr<detail::tcp_session_t<T>> ws) {
      auto session = std::make_shared<rpc_session_t<T>>(io_context_, rpc_config_);
      session->init(std::move(ws));
      if (on_session) {
        on_session(session);
      }
    };
    server_.on_handshake_error = [this](std::error_code ec) {
      if (on_handshake_error) on_handshake_error(ec);
    };
  }
#endif

  rpc_server_t(asio::io_context& io_context, const std::string& endpoint, rpc_config rpc_config = {})
      : io_context_(io_context), rpc_config_(rpc_config), server_(io_context, endpoint, rpc_config.to_tcp_config()) {
    static_assert(T == detail::socket_type::domain, "");
    server_.on_session = [this](std::weak_ptr<detail::tcp_session_t<T>> ws) {
      auto session = std::make_shared<rpc_session_t<T>>(io_context_, rpc_config_);
      session->init(std::move(ws));
      if (on_session) {
        on_session(session);
      }
    };
  }

 public:
  void start(bool loop = false) {
    server_.start(loop);
  }

 public:
  std::function<void(std::weak_ptr<rpc_session_t<T>>)> on_session;
  std::function<void(std::error_code)> on_handshake_error;

 private:
  asio::io_context& io_context_;
  rpc_config rpc_config_;
  detail::tcp_server_t<T> server_;
};

}  // namespace detail
}  // namespace asio_net
