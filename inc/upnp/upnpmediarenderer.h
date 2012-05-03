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

#ifndef UPNP_MEDIA_RENDERER_H
#define UPNP_MEDIA_RENDERER_H

#include <memory>

#include "upnp/upnpconnectionmanager.h"
#include "upnp/upnprenderingcontrol.h"
#include "upnp/upnpavtransport.h"

namespace upnp
{

class Item;
class Device;
class Client;
class AVTransport;
class ProtocolInfo;
class Resource;
struct ConnectionManager::ConnectionInfo;

class MediaRenderer
{
public:
    MediaRenderer(Client& cp);
    
    void setDevice(std::shared_ptr<Device> device);
    bool supportsPlayback(const Item& item, Resource& suggestedResource) const;
    
    std::string getPeerConnectionId() const;
    
    ConnectionManager& connectionManager();

    void setTransportItem(const ConnectionManager::ConnectionInfo& info, Resource& resource);
    void play(const ConnectionManager::ConnectionInfo& info);
    void stop(const ConnectionManager::ConnectionInfo& info);
    
    void activateEvents();
    void deactivateEvents();
    
private:
    std::shared_ptr<Device>         m_Device;
    std::vector<ProtocolInfo>       m_ProtocolInfo;
    
    Client&                         m_Client;
    ConnectionManager               m_ConnectionMgr;
    RenderingControl                m_RenderingControl;
    std::unique_ptr<AVTransport>    m_AVtransport;
};

}

#endif
