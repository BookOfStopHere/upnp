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

#include "utils/format.h"

namespace upnp
{

extern const char* RenderingControlServiceIdUrn;
extern const char* ConnectionManagerServiceIdUrn;
extern const char* AVTransportServiceIdUrn;
extern const char* ContentDirectoryServiceIdUrn;

extern const char* RenderingControlServiceMetadataUrn;
extern const char* ConnectionManagerServiceMetadataUrn;
extern const char* AVTransportServiceMetadataUrn;

enum class ErrorCode
{
    Success,
    Unexpected,
    InvalidArgument,
    BadRequest,
    PreconditionFailed,
    NetworkError,
    HttpError,
    EnumCount
};

const char* errorCodeToString(ErrorCode code);
int32_t errorCodeToInt(ErrorCode code);

class Status
{
public:
    Status() : m_errorCode(ErrorCode::Success) {}
    explicit Status(std::string msg) : m_errorCode(ErrorCode::Unexpected), m_message(std::move(msg)) {}
    explicit Status(const char* msg) : Status(std::string(msg)) {}
    explicit Status(ErrorCode ec) : m_errorCode(ec), m_message(errorCodeToString(ec)) {}

    Status(ErrorCode ec, const std::string& additionalInfo)
    : m_errorCode(ec)
    , m_message(fmt::format("{} ({})", errorCodeToString(ec), additionalInfo))
    {
    }

    template<typename... T>
    Status(const char* fmt, const T&... args) : Status(fmt::format(fmt, std::forward<const T>(args)...)) {}

    ErrorCode getErrorCode() const noexcept
    {
        return m_errorCode;
    }

    operator bool() const noexcept
    {
        return m_errorCode == ErrorCode::Success;
    }

    bool operator ==(const Status& other) const noexcept
    {
        return m_errorCode == other.m_errorCode;
    }

    const char* what() const noexcept
    {
        return m_message.c_str();
    }

private:
    ErrorCode m_errorCode;
    std::string m_message;
};

struct ServiceType
{
    enum Type
    {
        ContentDirectory,
        RenderingControl,
        ConnectionManager,
        AVTransport,
        Unknown
    };

    constexpr ServiceType() = default;
    constexpr ServiceType(Type t, uint32_t v) : type(t), version(v) {}

    bool operator==(const ServiceType& other) const { return type == other.type; }
    bool operator<(const ServiceType& other) const { return type < other.type; }


    Type      type = Unknown;
    uint32_t  version = 0;
};

struct DeviceType
{
    enum Type
    {
        MediaServer,
        MediaRenderer,
        InternetGateway,
        Unknown
    };

    DeviceType() = default;
    DeviceType(Type t, uint32_t v) : type(t), version(v) {}

    bool operator==(const DeviceType& other) const { return type == other.type; }
    bool operator<(const DeviceType& other) const { return type < other.type; }


    Type      type;
    uint32_t  version = 0;
};

enum class Property
{
    Id,
    ParentId,
    Title,
    Creator,
    Date,
    Description,
    Res,
    Class,
    Restricted,
    WriteStatus,
    RefId,
    ChildCount,
    CreateClass,
    SearchClass,
    Searchable,
    Artist,
    Album,
    AlbumArtist,
    AlbumArt,
    Icon,
    Genre,
    TrackNumber,
    Actor,
    StorageUsed,
    All,
    Unknown
};

enum class Class
{
    Container,
    VideoContainer,
    AudioContainer,
    ImageContainer,
    StorageFolder,
    Video,
    Audio,
    Image,
    Generic,
    Unknown
};

struct SubscriptionEvent
{
    std::string sid;
    std::string data;
    uint32_t sequence = 0;
};

Property propertyFromString(const char* data, size_t size);
Property propertyFromString(const std::string& name);
const char* toString(Property prop) noexcept;
const char* toString(Class c) noexcept;

const char* serviceTypeToTypeString(ServiceType type);
const char* serviceTypeToUrnTypeString(ServiceType type);
const char* serviceTypeToUrnIdString(ServiceType type);
const char* serviceTypeToUrnMetadataString(ServiceType type);

ServiceType::Type serviceTypeFromString(const std::string& name);
ServiceType serviceTypeUrnStringToService(const std::string& type);
ServiceType::Type serviceIdUrnStringToService(const std::string& type);

const char* deviceTypeToString(DeviceType type);
DeviceType stringToDeviceType(const std::string& type);

inline std::ostream& operator<<(std::ostream& os, const Status s)
{
    return os << errorCodeToInt(s.getErrorCode()) << " - " << s.what();
}

}

