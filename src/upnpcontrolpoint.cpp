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

#include "upnp/upnpcontrolpoint.h"

#include <chrono>
#include <condition_variable>

#include "upnp/upnpmediaserver.h"
#include "upnp/upnpmediarenderer.h"
#include "upnp/upnpprotocolinfo.h"
#include "upnp/upnpconnectionmanagerclient.h"
#include "upnp/upnpwebserver.h"

#include "utils/log.h"

using namespace utils;

namespace upnp
{

ControlPoint::ControlPoint(IClient& client)
: m_renderer(client)
, m_pWebServer(nullptr)
{
}

void ControlPoint::setWebserver(WebServer& webServer)
{
    m_pWebServer = &webServer;
    m_pWebServer->addVirtualDirectory("playlists");
}

void ControlPoint::setRendererDevice(const std::shared_ptr<Device>& dev)
{
    m_renderer.setDevice(dev);
    m_renderer.useDefaultConnection();
}

MediaRenderer& ControlPoint::getActiveRenderer()
{
    return m_renderer;
}

void ControlPoint::activate()
{
    m_renderer.activateEvents();
}

void ControlPoint::deactivate()
{
    m_renderer.deactivateEvents();
}

void ControlPoint::playItem(MediaServer& server, const Item& item)
{
    Resource resource;
    if (!m_renderer.supportsPlayback(item, resource))
    {
        throw Exception("The requested item is not supported by the renderer");
    }

    stopPlaybackIfNecessary();

    prepareConnection(server, resource);
    server.setTransportItem(resource);
    m_renderer.setTransportItem(resource);
    m_renderer.play();
}

void ControlPoint::playItemsAsPlaylist(upnp::MediaServer &server, const std::vector<Item> &items)
{
    if (items.empty())
    {
        throw Exception("No items provided for playback");
    }

    if (items.size() == 1)
    {
        return playItem(server, items.front());
    }

    // create a playlist from the provided items
    throwOnMissingWebserver();

    std::stringstream playlist;
    for (auto& item : items)
    {
        Resource res;
        if (m_renderer.supportsPlayback(item, res))
        {
            playlist << res.getUrl() << std::endl;
        }
    }

    std::string filename = generatePlaylistFilename();
    m_pWebServer->addFile("playlists", filename, "audio/m3u", playlist.str());
    playItem(server, createPlaylistItem(filename));
}

void ControlPoint::queueItem(MediaServer& /*server*/, const Item& item)
{
    Resource resource;
    if (!m_renderer.supportsPlayback(item, resource))
    {
        throw Exception("The requested item is not supported by the renderer");
    }

    m_renderer.setNextTransportItem(resource);
}

void ControlPoint::queueItemsAsPlaylist(upnp::MediaServer &server, const std::vector<Item>& items)
{
    if (items.empty())
    {
        throw Exception("No items provided for queueing");
    }

    if (items.size() == 1)
    {
        return playItem(server, items.front());
    }

    // create a playlist from the provided items
    throwOnMissingWebserver();

    std::stringstream playlist;
    for (auto& item : items)
    {
        Resource res;
        if (m_renderer.supportsPlayback(item, res))
        {
            playlist << res.getUrl() << std::endl;
        }
    }

    std::string filename = generatePlaylistFilename();
    m_pWebServer->addFile("playlists", filename, "audio/m3u", playlist.str());
    queueItem(server, createPlaylistItem(filename));
}

void ControlPoint::stopPlaybackIfNecessary()
{
    try
    {
        //if (m_renderer.isActionAvailable(MediaRenderer::Action::Stop))
        //{
            m_renderer.stop();
        //}
    } catch (std::exception&) {}
}

void ControlPoint::throwOnMissingWebserver()
{
    if (!m_pWebServer)
    {
        throw Exception("A web server must be available to serve playlists");
    }
}

std::string ControlPoint::generatePlaylistFilename()
{
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    std::stringstream playlistFilename;
    playlistFilename << "playlist-" << now.time_since_epoch().count() << ".m3u";

    return playlistFilename.str();
}

Item ControlPoint::createPlaylistItem(const std::string& filename)
{
    Resource res;
    res.setUrl(m_pWebServer->getWebRootUrl() + "playlists/" + filename);
    res.setProtocolInfo(ProtocolInfo("http-get:*:audio/m3u:*"));

    auto playlistItem = Item();
    playlistItem.addResource(res);
    return playlistItem;
}

void ControlPoint::prepareConnection(MediaServer& server, Resource& resource)
{
    if (m_renderer.supportsConnectionPreparation())
    {
        if (server.supportsConnectionPreparation())
        {
            server.prepareConnection(resource, m_renderer.getPeerConnectionManager(), ConnectionManager::UnknownConnectionId);
        }

        m_renderer.prepareConnection(resource, server.getPeerConnectionManager(), server.getConnectionId());
    }
    else
    {
        server.useDefaultConnection();
        m_renderer.useDefaultConnection();
    }
}

}

