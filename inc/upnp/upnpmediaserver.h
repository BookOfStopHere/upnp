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

#ifndef UPNP_MEDIA_SERVER_H
#define UPNP_MEDIA_SERVER_H

#include <memory>

#include "upnp/upnpconnectionmanager.h"
#include "upnp/upnpcontentdirectory.h"

#include "utils/threadpool.h"

namespace upnp
{
    
class Item;
class Device;
class Client;

class MediaServer
{
public:
    static const std::string rootId;

    enum class SortMode
    {
        Ascending,
        Descending
    };

    MediaServer(const Client& client);
    ~MediaServer();
    
    void setDevice(std::shared_ptr<Device> device);
    
    void abort();
    
    bool canSearchForProperty(Property prop) const;
    bool canSortOnProperty(Property prop) const;
    const std::vector<Property>& getSearchCapabilities() const;
    const std::vector<Property>& getSortCapabilities() const;
    
    void getItemsInContainer(Item& container, utils::ISubscriber<Item>& subscriber, Property sort = Property::Unknown, SortMode mode = SortMode::Ascending);
    void getItemsInContainerAsync(Item& container, utils::ISubscriber<Item>& subscriber, Property sort = Property::Unknown, SortMode mode = SortMode::Ascending);
    void getContainersInContainer(Item& container, utils::ISubscriber<Item>& subscriber, Property sort = Property::Unknown, SortMode mode = SortMode::Ascending);
    void getContainersInContainerAsync(Item& container, utils::ISubscriber<Item>& subscriber, Property sort = Property::Unknown, SortMode mode = SortMode::Ascending);
    void getAllInContainer(Item& container, utils::ISubscriber<Item>& subscriber, Property sort = Property::Unknown, SortMode mode = SortMode::Ascending);
    void getAllInContainerAsync(Item& container, utils::ISubscriber<Item>& subscriber, Property sort = Property::Unknown, SortMode mode = SortMode::Ascending);
    
    uint32_t search(Item& container, const std::string& criteria, utils::ISubscriber<Item>& subscriber);
    uint32_t search(Item& container, const std::map<Property, std::string>& criteria, utils::ISubscriber<Item>& subscriber);
    void searchAsync(Item& container, const std::string& criteria, utils::ISubscriber<Item>& subscriber);
    void searchAsync(Item& container, const std::map<Property, std::string>& criteria, utils::ISubscriber<Item>& subscriber);
    
    void getMetaData(Item& item);
    void getMetaDataAsync(std::shared_ptr<Item> item, utils::ISubscriber<std::shared_ptr<Item>>& subscriber);
    
private:
    void performBrowseRequest(ContentDirectory::BrowseType type, Item& container, utils::ISubscriber<Item>& subscriber, Property sort = Property::Unknown, SortMode = SortMode::Ascending);
    void performBrowseRequestThread(ContentDirectory::BrowseType type, const Item& item, utils::ISubscriber<Item>& subscriber, Property sort = Property::Unknown, SortMode = SortMode::Ascending);
    template <typename T>
    void searchThread(Item& container, const T& criteria, utils::ISubscriber<Item>& subscriber);
    void getMetaDataThread(std::shared_ptr<Item> item, utils::ISubscriber<std::shared_ptr<Item>>& subscriber);

    std::shared_ptr<Device>     m_Device;
    std::vector<ProtocolInfo>   m_ProtocolInfo;
    
    ContentDirectory            m_ContentDirectory;
    ConnectionManager           m_ConnectionMgr;
    
    utils::ThreadPool           m_ThreadPool;
    bool                        m_Abort;
};
    
}

#endif
