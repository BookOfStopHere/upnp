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

#pragma once

#include <chrono>
#include <vector>
#include <cinttypes>
#include <functional>

#include "upnp/upnp.http.client.h"
//#include "upnp/upnpclientinterface.h"

namespace uv
{
    class Loop;
}

namespace upnp
{

namespace gena
{

class Server;

}

class Action;

class Client2
{
public:
    Client2(uv::Loop& loop);
    virtual ~Client2();

    virtual void initialize(const std::string& interfaceName, int32_t port);
    virtual void uninitialize();

    virtual std::string getIpAddress() const;
    virtual int32_t getPort() const;

    virtual void subscribeToService(const std::string& publisherUrl, std::chrono::seconds timeout, std::function<void(int32_t status, std::string subId, std::chrono::seconds timeout)> cb);
    virtual void unsubscribeFromService(const std::string& publisherUrl, const std::string& subscriptionId, std::function<void(int32_t status)> cb);

    virtual void sendAction(const Action& action, std::function<void(int32_t status, std::string actionResult)> cb);

private:
    uv::Loop& m_loop;
    http::Client m_http;
    std::unique_ptr<gena::Server> m_eventServer;
    //std::vector<IServiceSubscriber*> m_serviceSubscriptions;
};
}