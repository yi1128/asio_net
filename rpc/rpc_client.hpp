#pragma once

#include <utility>

#include "RpcCore.hpp"
#include "detail/noncopyable.hpp"
#include "rpc_session.hpp"
#include "tcp_client.hpp"

namespace asio_net {

class rpc_client : noncopyable {
 public:
  explicit rpc_client(asio::io_context& io_context, uint32_t max_body_size = UINT32_MAX)
      : io_context_(io_context), client_(io_context, PackOption::ENABLE, max_body_size) {
    client_.on_open = [this]() {
      auto rpc = RpcCore::Rpc::create();

      rpc->setTimer([this](uint32_t ms, RpcCore::Rpc::TimeoutCb cb) {
        auto timer = std::make_shared<asio::steady_timer>(io_context_);
        timer->expires_after(std::chrono::milliseconds(ms));
        timer->async_wait([timer = std::move(timer), cb = std::move(cb)](const std::error_code&) {
          cb();
        });
      });

      rpc->getConn()->sendPackageImpl = [this](std::string data) {
        client_.send(std::move(data));
      };

      client_.on_data = [rpc](std::string data) {
        rpc->getConn()->onRecvPackage(std::move(data));
      };

      on_open(rpc);
    };

    client_.on_close = [this] {
      client_.on_data = nullptr;
      on_close();
    };

    client_.on_open_failed = [this](const std::error_code& ec) {
      if (on_open_failed) on_open_failed(ec);
    };
  }

  void open(const std::string& ip, uint16_t port) {
    client_.open(ip, port);
  }

  void close() {
    client_.close();
  }

 public:
  std::function<void(std::shared_ptr<RpcCore::Rpc>)> on_open;
  std::function<void()> on_close;
  std::function<void(std::error_code)> on_open_failed;

 private:
  asio::io_context& io_context_;
  tcp_client client_;
};

}  // namespace asio_net
