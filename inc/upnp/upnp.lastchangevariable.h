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

#include <string>
#include <cinttypes>
#include <unordered_map>
#include <chrono>

#include "upnp/asio.h"
#include "upnp/upnp.types.h"
#include "upnp/upnpservicevariable.h"

namespace upnp
{

class LastChangeVariable
{
public:
    LastChangeVariable(asio::io_service& io, ServiceType type, std::chrono::milliseconds minEventInterval);

    void addChangedVariable(uint32_t instanceId, const ServiceVariable& var);

    std::function<void(const std::string&)> LastChangeEvent;

private:
    void variableThread();
    void createLastChangeEvent();

    bool                                                        m_timerScheduled;
    std::unordered_map<uint32_t, std::vector<ServiceVariable>>  m_changedVariables;
    std::chrono::milliseconds                                   m_minInterval;
    std::chrono::steady_clock::time_point                       m_lastUpdate;
    std::string                                                 m_eventMetaNamespace;
    asio::steady_timer                                          m_timer;
};

}
