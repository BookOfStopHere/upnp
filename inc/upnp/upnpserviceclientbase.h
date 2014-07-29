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

#ifndef UPNP_SERVICE_H
#define UPNP_SERVICE_H

#include "utils/log.h"
#include "utils/signal.h"

#include "upnp/upnpclientinterface.h"
#include "upnp/upnpdevice.h"
#include "upnp/upnpxmlutils.h"

#include <upnp/upnp.h>

#include <set>
#include <map>
#include <vector>
#include <memory>

namespace upnp
{
    
class IClient;

template <typename ActionType, typename VariableType>
class ServiceClientBase
{
public:
    utils::Signal<void(VariableType, const std::map<VariableType, std::string>&)> StateVariableEvent;

    ServiceClientBase(IClient& client)
    : m_Client(client)
    {
    }
    
    virtual ~ServiceClientBase()
    {
        try
        {
            unsubscribe();
        }
        catch (std::exception& e)
        {
            utils::log::error(e.what());
        }
    }
    
    virtual void setDevice(const std::shared_ptr<Device>& device)
    {
        if (device->implementsService(getType()))
        {
            m_Service = device->m_Services[getType()];
            parseServiceDescription(m_Service.m_SCPDUrl);
        }
    }
    
    void subscribe()
    {
        try { unsubscribe(); }
        catch (std::exception& e) { utils::log::warn(e.what()); }
        
        m_Client.UPnPEventOccurredEvent.connect(std::bind(&ServiceClientBase::eventOccurred, this, std::placeholders::_1), this);
        m_Client.subscribeToService(m_Service.m_EventSubscriptionURL, getSubscriptionTimeout(), &ServiceClientBase::eventCb, this);
    }
    
    void unsubscribe()
    {
        if (!m_SubscriptionId.empty())
        {
            m_Client.UPnPEventOccurredEvent.disconnect(this);
            m_Client.unsubscribeFromService(m_SubscriptionId);
            m_SubscriptionId.clear();
        }
    }
    
    bool supportsAction(ActionType action) const
    {
        return m_SupportedActions.find(action) != m_SupportedActions.end();
    }
    
    virtual ActionType actionFromString(const std::string& action) const = 0;
    virtual std::string actionToString(ActionType action) const  = 0;
    virtual VariableType variableFromString(const std::string& var) const  = 0;
    virtual std::string variableToString(VariableType var) const  = 0;
    
protected:
    virtual void parseServiceDescription(const std::string& descriptionUrl)
    {
        try
        {
            xml::Document doc = m_Client.downloadXmlDocument(descriptionUrl);
            for (auto& action : xml::utils::getActionsFromDescription(doc))
            {
                try
                {
                    m_SupportedActions.insert(actionFromString(action));
                }
                catch (std::exception& e)
                {
                    utils::log::error(e.what());
                }
            }
            
            m_StateVariables = xml::utils::getStateVariablesFromDescription(doc);
        }
        catch (std::exception& e)
        {
            utils::log::error(e.what());
        }
    }
    
    void eventOccurred(Upnp_Event* pEvent)
    {
        if (pEvent->Sid == m_SubscriptionId)
        {
            try
            {
                xml::Document doc(pEvent->ChangedVariables, xml::Document::NoOwnership);
                xml::Element propertySet = doc.getFirstChild();
                for (xml::Element property : propertySet.getChildNodes())
                {
                    for (xml::Element var : property.getChildNodes())
                    {
                        try
                        {
                            VariableType changedVar = variableFromString(var.getName());
                            
                            xml::Document changeDoc(var.getValue());
                            xml::Element eventNode = changeDoc.getFirstChild();
                            xml::Element instanceIDNode = eventNode.getChildElement("InstanceID");
                            
                            std::map<VariableType, std::string> vars;
                            for (xml::Element elem : instanceIDNode.getChildNodes())
                            {
                                auto str = elem.getAttribute("val");
                                utils::log::debug("%s %s", elem.getName(), elem.getAttribute("val"));
                                vars.insert(std::make_pair(variableFromString(elem.getName()), elem.getAttribute("val")));
                            }
                            
                            // let the service implementation process the event if necessary
                            handleStateVariableEvent(changedVar, vars);
                            
                            // notify clients
                            StateVariableEvent(changedVar, vars);
                        }
                        catch (std::exception& e)
                        {
                            utils::log::warn("Unknown event variable ignored: %s", e.what());
                            utils::log::debug(var.toString());
                        }
                    }
                }
            }
            catch (std::exception& e)
            {
                utils::log::error("Failed to parse event: %s", e.what());
            }
        }
    }
    
    xml::Document executeAction(ActionType actionType)
    {
        return executeAction(actionType, std::map<std::string, std::string> {});
    }
    
    xml::Document executeAction(ActionType actionType, const std::map<std::string, std::string>& args)
    {
        Action action(actionToString(actionType), m_Service.m_ControlURL, getType());
        for (auto& arg : args)
        {
            action.addArgument(arg.first, arg.second);
        }
        
        try
        {
            return m_Client.sendAction(action);
        }
        catch (UPnPException& e)
        {
            handleUPnPResult(e.getErrorCode());
        }
        
        assert(false);
        return xml::Document();
    }
    
    static int eventCb(Upnp_EventType eventType, void* pEvent, void* pInstance)
    {
        auto rc = reinterpret_cast<ServiceClientBase<ActionType, VariableType>*>(pInstance);
        
        switch (eventType)
        {
            case UPNP_EVENT_SUBSCRIBE_COMPLETE:
            {
                auto pSubEvent = reinterpret_cast<Upnp_Event_Subscribe*>(pEvent);
                if (pSubEvent->ErrCode != UPNP_E_SUCCESS)
                {
                    utils::log::error("Error in Event Subscribe Callback: %d", pSubEvent->ErrCode);
                }
                else
                {
                    if (pSubEvent->Sid)
                    {
                        rc->m_SubscriptionId = pSubEvent->Sid;
                        
#ifdef DEBUG_SERVICE_SUBSCRIPTIONS
                        utils::log::debug("Subscription complete: %s", rc->m_SubscriptionId);
#endif
                    }
                    else
                    {
                        rc->m_SubscriptionId.clear();
                        utils::log::error("Subscription id for device is empty");
                    }
                }
                break;
            }
            case UPNP_EVENT_AUTORENEWAL_FAILED:
            case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
            {
                auto pSubEvent = reinterpret_cast<Upnp_Event_Subscribe*>(pEvent);
                
                try
                {
                    int32_t timeout = rc->getSubscriptionTimeout();
                    rc->m_SubscriptionId = rc->m_Client.subscribeToService(pSubEvent->PublisherUrl, timeout);

#ifdef DEBUG_SERVICE_SUBSCRIPTIONS
                    utils::log::debug("Service subscription renewed: %s", rc->m_SubscriptionId);
#endif
                }
                catch (std::exception& e)
                {
                    utils::log::error("Failed to renew event subscription: %s", e.what());
                }
                break;
            }
            case UPNP_EVENT_RENEWAL_COMPLETE:
#ifdef DEBUG_SERVICE_SUBSCRIPTIONS
                utils::log::debug("Event subscription renewal complete");
#endif
                break;
            default:
                utils::log::info("Unhandled action: %d", eventType);
                break;
        }
        
        return 0;
    }
    
    virtual ServiceType getType() = 0;
    virtual int32_t getSubscriptionTimeout() = 0;
    virtual void handleStateVariableEvent(VariableType changedVariable, const std::map<VariableType, std::string>& variables) {}
    virtual void handleUPnPResult(int errorCode) = 0;

    std::vector<StateVariable>              m_StateVariables;
    
private:
    IClient&                                m_Client;
    Service                                 m_Service;
    std::set<ActionType>                    m_SupportedActions;
    std::string                             m_SubscriptionId;
};
    
}

#endif