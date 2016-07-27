//    Copyright (C) 2012 Dirk Vanden Boer <dirk.vdb@gmail.com>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "upnp.http.client.h"

#include <cassert>
#include <stdexcept>
#include <sstream>

#include "utils/format.h"
#include "utils/log.h"
#include "utils/stringoperations.h"

#include "upnp.soap.parseutils.h"

#include "URI.h"
#include "stringview.h"


namespace std
{

error_code make_error_code(upnp::http::error::ErrorCode e) noexcept
{
    return error_code(static_cast<int>(e), upnp::http::error::HttpErrorCategory::get());
}

template<>
struct is_error_code_enum<upnp::http::error::ErrorCode> : std::true_type
{
};

}


namespace upnp
{
namespace http
{

using namespace asio;
using namespace utils;
using namespace std::string_literals;
using namespace std::chrono_literals;

static const std::string s_delimiter = "\r\n";

namespace
{

std::error_code convertError(const std::error_code& error)
{
    if (error.value() == asio::error::timed_out)
    {
        return std::make_error_code(error::Timeout);
    }

    log::error("Error performing Http call: {}", error.message());
    return std::make_error_code(error::NetworkError);
}

bool invokeCallbackOnError(const asio::error_code& error, const std::function<void(const std::error_code&)>& cb)
{
    if (!error)
    {
        return false;
    }

    cb(convertError(error));
    return true;
}

}

Client::Client(asio::io_service& io)
: m_timer(io)
, m_socket(io)
, m_timeout(60000ms)
, m_parser(Type::Response)
{
}

void Client::reset()
{
    m_parser.reset();
}

void Client::setTimeout(std::chrono::milliseconds timeout) noexcept
{
    m_timeout = timeout;
}

void Client::addHeader(std::string header)
{
    m_headers.emplace_back(std::move(header));
}

void Client::setUrl(const std::string& url)
{
    m_uri = URI(url);
}

void Client::performRequest(const ip::tcp::endpoint& addr, std::function<void(const asio::error_code&)> cb)
{
    performRequest(addr, "", cb);
}

const std::string& Client::getResponseHeaderValue(const char* headerValue)
{
    return m_parser.headerValue(headerValue);
}

std::string Client::getResponseBody()
{
    return m_parser.stealBody();
}

uint32_t Client::getStatus()
{
    return m_parser.getStatus();
}

void Client::performRequest(const ip::tcp::endpoint& addr, const std::string& body, std::function<void(const asio::error_code&)> cb)
{
    std::vector<const_buffer> buffers;
    buffers.emplace_back(buffer(m_request));

    for (auto& h : m_headers)
    {
        buffers.emplace_back(buffer(h));
    }

    buffers.emplace_back(buffer(s_delimiter));

    if (!body.empty())
    {
        buffers.emplace_back(buffer(body));
    }

    performRequest(addr, buffers, cb);
}

void Client::performRequest(const ip::tcp::endpoint& addr, const std::vector<const_buffer>& buffers, std::function<void(const asio::error_code&)> cb)
{
    reset();

    std::error_code error;
    m_socket.close(error);
    if (error)
    {
        cb(error);
        return;
    }

    m_socket.open(addr.protocol(), error);
    if (error)
    {
        cb(error);
        return;
    }

    m_timer.expires_from_now(m_timeout);
    m_socket.async_connect(addr, [this, cb, buffers] (const asio::error_code& error) {
        if (!m_socket.is_open())
        {
            cb(std::make_error_code(error::Timeout));
            return;
        }

        m_timer.cancel();

        if (invokeCallbackOnError(error, cb))
        {
            return;
        }

        async_write(m_socket, buffers, [this, cb] (const std::error_code& error, size_t) {
            if (invokeCallbackOnError(error, cb))
            {
                return;
            }

            receiveData(cb);
        });
    });

    m_timer.async_wait([this] (const asio::error_code& ec) { checkTimeout(ec); });
}

void Client::receiveData(std::function<void(const std::error_code&)> cb)
{
    m_timer.expires_from_now(m_timeout);
    m_socket.async_receive(buffer(m_buffer), [this, cb] (const asio::error_code& error, size_t bytesReceived) {
        m_timer.cancel();
        if (invokeCallbackOnError(error, cb))
        {
            return;
        }

        try
        {
            auto processed = m_parser.parse(m_buffer.data(), bytesReceived);
            if (processed != bytesReceived)
            {
                log::warn("Failed to parser received http data");
                cb(std::make_error_code(error::InvalidResponse));
            }
            else
            {
                if (!m_parser.isCompleted())
                {
                    receiveData(cb);
                }
                else if (strncasecmp(m_parser.headerValue("Connection").c_str(), "close", 5) == 0)
                {
                    std::error_code error;
                    m_socket.close(error);
                }
            }
        }
        catch (const std::exception& e)
        {
            log::error("Failed to parse http response: {}", e.what());
            cb(std::make_error_code(error::InvalidResponse));
        }
    });
}

static std::string methodToString(Method m)
{
    switch (m)
    {
    case Method::Head:          return "HEAD";
    case Method::Notify:        return "NOTIFY";
    case Method::Search:        return "M-SEARCH";
    case Method::Subscribe:     return "SUBSCRIBE";
    case Method::Unsubscribe:   return "UNSUBSCRIBE";
    case Method::Get:           return "GET";
    case Method::Post:          return "POST";
    default:
        throw std::invalid_argument("Invalid http method provided");
    }
}

void Client::setMethodType(Method method)
{
    m_request = fmt::format("{} {} HTTP/1.1\r\n", methodToString(method), m_uri.getPath());
    m_headers.emplace_back(fmt::format("Host:{}\r\n", m_uri.getAuthority()));
    //m_headers.emplace_back("Connection:Keep-alive\r\n");
}

void Client::perform(Method method, std::function<void(const std::error_code&, std::string data)> cb)
{
    setMethodType(method);

    if (method == Method::Head)
    {
        m_parser.setHeadersCompletedCallback([this, cb] () {
            cb(std::make_error_code(error::ErrorCode(m_parser.getStatus())), std::string());
        });
    }
    else
    {
        m_parser.setCompletedCallback([this, cb] () {
            cb(std::make_error_code(error::ErrorCode(m_parser.getStatus())), m_parser.stealBody());
        });
    }

    performRequest(ip::tcp::endpoint(ip::address::from_string(m_uri.getHost()), m_uri.getPort()), [cb] (const asio::error_code& error) {
        if (error)
        {
            cb(error, "");
        }
    });
}

void Client::perform(Method method, const std::string& body, std::function<void(const std::error_code&, std::string data)> cb)
{
    setMethodType(method);

    if (method == Method::Head)
    {
        m_parser.setHeadersCompletedCallback([this, cb] () {
            cb(std::make_error_code(error::ErrorCode(m_parser.getStatus())), std::string());
        });
    }
    else
    {
        m_parser.setCompletedCallback([this, cb] () {
            cb(std::make_error_code(error::ErrorCode(m_parser.getStatus())), m_parser.stealBody());
        });
    }

    performRequest(ip::tcp::endpoint(ip::address::from_string(m_uri.getHost()), m_uri.getPort()), body, [cb] (const asio::error_code& error) {
        if (error)
        {
            cb(error, "");
        }
    });
}

void Client::perform(Method method, uint8_t* data, std::function<void(const std::error_code&, uint8_t*)> cb)
{
    setMethodType(method);

    if (method == Method::Head)
    {
        m_parser.setHeadersCompletedCallback([this, cb, data] () {
            cb(std::make_error_code(error::ErrorCode(m_parser.getStatus())), data);
        });
    }
    else
    {
        m_parser.setCompletedCallback([this, cb, data] () {
            auto body = m_parser.stealBody();
            memcpy(data, body.data(), body.size());
            cb(std::make_error_code(error::ErrorCode(m_parser.getStatus())), data);
        });
    }

    m_parser.setCompletedCallback([this, cb, data] () {
        auto body = m_parser.stealBody();
        memcpy(data, body.data(), body.size());
        cb(std::make_error_code(error::ErrorCode(m_parser.getStatus())), data);
    });

    performRequest(ip::tcp::endpoint(ip::address::from_string(m_uri.getHost()), m_uri.getPort()), [cb] (const std::error_code& error) {
        if (error)
        {
            cb(error, nullptr);
        }
    });
}

void Client::checkTimeout(const asio::error_code& ec)
{
    if (ec.value() == asio::error::operation_aborted)
    {
        return;
    }

    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (m_timer.expires_at() <= std::chrono::steady_clock::now())
    {
        // The deadline has passed. The socket is closed so that any outstanding
        // asynchronous operations are cancelled.
        std::error_code error;
        m_socket.close(error);

        // There is no longer an active deadline. The expiry is set to positive
        // infinity so that the actor takes no action until a new deadline is set.
        m_timer.expires_at(std::chrono::steady_clock::time_point::max());
        return;
    }

    m_timer.async_wait([this] (const asio::error_code& ec) { checkTimeout(ec); });
}

}
}
