/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#ifndef AZ_UNITY_BUILD

#include <GridMate/Carrier/SocketDriver.h>
#include <GridMate/Carrier/Utils.h>
#include <AzCore/Socket/AzSocket.h>

/**
 * Enable this define if you want to print all packets and addresses
 * to unbound connections. Unbound convention is when you don't have the full
 * address to receive or send data.
 */
//#define AZ_LOG_UNBOUND_SEND_RECEIVE

#include <GridMate/Containers/unordered_set.h>
#include <GridMate/String/string.h>

#include <AzCore/std/chrono/types.h>
#include <AzCore/std/string/conversions.h>

#if defined(AZ_PLATFORM_WINDOWS)
#   include <AzCore/PlatformIncl.h>
#   include <WinSock2.h>
#   include <ws2tcpip.h>
#   define SO_NBIO          FIONBIO
#   define AZ_EWOULDBLOCK   WSAEWOULDBLOCK
#   define AZ_EINPROGRESS   WSAEINPROGRESS
#   define AZ_ECONNREFUSED  WSAECONNREFUSED
#   define AZ_EALREADY      WSAEALREADY
#   define AZ_EISCONN       WSAEISCONN
#   define AZ_ENETUNREACH   WSAENETUNREACH
#   define AZ_ETIMEDOUT     WSAETIMEDOUT
#elif defined(AZ_PLATFORM_X360)
// Redacted
#elif defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_ANDROID)
#   include <sys/socket.h>
#   include <sys/select.h>
#   include <arpa/inet.h>
#   include <netdb.h>
#   include <errno.h>
#   include <unistd.h>
#   include <sys/ioctl.h>

#   define closesocket(_s) close(_s)
#   define AZ_EWOULDBLOCK EWOULDBLOCK
#   define AZ_EINPROGRESS EINPROGRESS
#   define AZ_ECONNREFUSED ECONNREFUSED
#   define AZ_EALREADY EALREADY
#   define AZ_EISCONN EISCONN
#   define AZ_ENETUNREACH ENETUNREACH
#   define AZ_ETIMEDOUT ETIMEDOUT
#   define SO_NBIO FIONBIO
#   define ioctlsocket  ioctl
#else
#error Platform not supported.
#endif

#define AZ_SOCKET_WAKEUP_MSG_TYPE   char
#define AZ_SOCKET_WAKEUP_MSG_VALUE  'G'

#if !defined(AZ_SOCKET_IPV6_SUPPORT) && !defined(AZ_PLATFORM_APPLE)

#   define AI_PASSIVE                  0x00000001
#   define AI_CANONNAME                0x00000002
#   define AI_NUMERICHOST              0x00000004
#   define AI_NUMERICSERV              0x00000008
#   ifndef AF_INET6
#       define AF_INET6             23
#   endif
#   ifndef AF_UNSPEC
#       define AF_UNSPEC            0
#   endif

#   define IPPROTO_IPV6             41
#   define IPV6_V6ONLY              27
#   define IPV6_ADD_MEMBERSHIP      12

namespace GridMate
{
    int getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res)
    {
        static addrinfo info;
        static sockaddr_in sockAddr;
        AZ_Assert(hints != nullptr, "Missing hints");
        AZ_Assert(hints->ai_family == AF_UNSPEC || hints->ai_family == AF_INET, "This function supports only AF_INET or AF_UNSPEC");
        (void)node;
        (void)service;
        (void)res;
        if (res)
        {
            *res = &info;
        }

        memset(&info, 0, sizeof(info));
        info.ai_family = AF_INET;
        info.ai_addr = reinterpret_cast<sockaddr*>(&sockAddr);
        info.ai_addrlen = sizeof(sockAddr);
        info.ai_socktype = hints->ai_socktype;
        info.ai_protocol = IPPROTO_UDP;

        memset(&sockAddr, 0, sizeof(sockAddr));
        sockAddr.sin_family = AF_INET;
        switch (hints->ai_flags)
        {
        case AI_PASSIVE:
        case AI_CANONNAME:
        {
            if (node && strlen(node) >= 7)
            {
                sockAddr.sin_addr.s_addr = inet_addr(node);
            }
            else
            {
                sockAddr.sin_addr.s_addr = INADDR_ANY;
            }

            if (service)
            {
                sockAddr.sin_port = htons(static_cast<unsigned short>(atoi(service)));
            }
        } break;
        default:
            AZ_Assert(false, "flag %d not supported in emulation", hints->ai_flags);
        }
        return 0;
    }

    void freeaddrinfo(struct addrinfo* res)
    {
        (void)res;
    }
} // namespace GridMate
#endif // AZ_SOCKET_IPV6_SUPPORT

namespace GridMate
{
#ifdef AZ_PLATFORM_WINDOWS
    const char* inet_ntop(int af, const void* src, char* dst, socklen_t size)
    {
#if (_WIN32_WINNT <  0x0600) // Windows XP
#pragma warning(push)
#pragma warning(disable:4996)
        AZ_Assert(af == AF_INET6 || af == AF_INET, "This function supports only AF_INET or AF_INET6");
        union
        {
            sockaddr_in     sockAddr;
            sockaddr_in6    sockAddr6;
        };
        DWORD sockAddrSize;
        if (af == AF_INET6)
        {
            sockAddrSize = sizeof(sockAddr6);
            memset(&sockAddr6, 0, sockAddrSize);
            memcpy(&(sockAddr6.sin6_addr), src, sizeof(sockAddr6.sin6_addr));
            sockAddr6.sin6_family = static_cast<short>(af);
        }
        else
        {
            sockAddrSize = sizeof(sockAddr);
            memset(&sockAddr, 0, sockAddrSize);
            memcpy(&(sockAddr.sin_addr), src, sizeof(sockAddr.sin_addr));
            sockAddr.sin_family = static_cast<short>(af);
        }
        if (WSAAddressToString((struct sockaddr*)&sockAddr, sockAddrSize, 0, dst, (LPDWORD)&size) != 0)
        {
            AZ_Error("GridMate", false, "WSAAddressToString() : %d\n", WSAGetLastError());
            return nullptr;
        }
        return dst;
#pragma warning(pop)
#else
        return ::inet_ntop(af, const_cast<void*>(src), dst, size);
#endif // (_WIN32_WINNT <  0x0600) // Windows XP
    }
#endif // AZ_PLATFORM_WINDOWS

    bool IsValidSocket(SocketDriverCommon::SocketType s)
    {
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_X360) || defined(AZ_PLATFORM_XBONE)
        return s != INVALID_SOCKET;
#elif defined(AZ_PLATFORM_PS3) || defined(AZ_PLATFORM_PS4) || defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_ANDROID)
        return s >= 0;
#else
#   error Not implemented
#endif
    }

    SocketDriverCommon::SocketType GetInvalidSocket()
    {
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_X360) || defined(AZ_PLATFORM_XBONE)
        return INVALID_SOCKET;
#elif defined(AZ_PLATFORM_PS3) || defined(AZ_PLATFORM_PS4) || defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_ANDROID)
        return -1;
#else
#   error Not implemented
#endif
    }

    bool IsSocketError(AZ::s64 result)
    {
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_X360) || defined(AZ_PLATFORM_XBONE)
        return result == SOCKET_ERROR;
#elif defined(AZ_PLATFORM_PS3) || defined(AZ_PLATFORM_PS4) || defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_ANDROID)
        return result < 0;
#else
#   error Not implemented
#endif
    }

    int GetSocketError()
    {
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_X360) || defined(AZ_PLATFORM_XBONE)
        return WSAGetLastError();
#elif defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_ANDROID)
        return errno;
#else
#   error Not implemented
#endif
    }

    namespace SocketOperations
    {
        //=========================================================================
        // HostToNetLong
        // [3/23/2017]
        //=========================================================================
        AZ::u32 HostToNetLong(AZ::u32 hstLong)
        {
            return htonl(hstLong);
        }
        
        //=========================================================================
        // NetToHostLong
        // [3/23/2017]
        //=========================================================================
        AZ::u32 NetToHostLong(AZ::u32 netLong)
        {
            return ntohl(netLong);
        }
        
        //=========================================================================
        // HostToNetShort
        // [3/23/2017]
        //=========================================================================
        AZ::u16 HostToNetShort(AZ::u16 hstShort)
        {
            return htons(hstShort);
        }

        //=========================================================================
        // NetToHostShort
        // [3/23/2017]
        //=========================================================================
        AZ::u16 NetToHostShort(AZ::u16 netShort)
        {
            return ntohs(netShort);
        }

        //=========================================================================
        // CreateSocket
        // [4/4/2017]
        //=========================================================================
        SocketDriverCommon::SocketType CreateSocket(bool isDatagram, Driver::BSDSocketFamilyType familyType)
        {
            int addressFamily = (familyType == Driver::BSDSocketFamilyType::BSD_AF_INET6) ? AF_INET6 : AF_INET;
            int socketType = isDatagram ? SOCK_DGRAM : SOCK_STREAM;
            int protocol = isDatagram ? IPPROTO_UDP : IPPROTO_TCP;
            return socket(addressFamily, socketType, protocol);
        }

        //=========================================================================
        // SetSocketOptionValue
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode SetSocketOptionValue(SocketDriverCommon::SocketType sock, SocketOption option, const char* optval, AZ::s32 optlen)
        {
            AZ::s32 optionName;
            switch (option)
            {
            case SocketOption::NonBlockingIO:
                optionName = SO_NBIO;
                break;
            case SocketOption::ReuseAddress:
                optionName = SO_REUSEADDR;
                break;
            case SocketOption::KeepAlive:
                optionName = SO_KEEPALIVE;
                break;
            case SocketOption::Broadcast:
                optionName = SO_BROADCAST;
                break;
            case SocketOption::SendBuffer:
                optionName = SO_SNDBUF;
                break;
            case SocketOption::ReceiveBuffer:
                optionName = SO_RCVBUF;
                break;
            default:
                optionName = -1;
                AZ_Error("GridMate", false, "Unsupported socket option: %d with error:%d", option, GetSocketError());
                return Driver::EC_SOCKET_SOCK_OPT;
            }
            AZ::s64 sockResult = setsockopt(sock, SOL_SOCKET, optionName, optval, optlen);
            AZ_Error("GridMate", sockResult == 0, "Socket option: %d failed to set. Error:%d", option, GetSocketError());
            return (sockResult == 0) ? Driver::EC_OK : Driver::EC_SOCKET_SOCK_OPT;
        }

        //=========================================================================
        // SetSocketOptionBoolean
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode SetSocketOptionBoolean(SocketDriverCommon::SocketType sock, SocketOption option, bool enable)
        {
            bool value = enable;
            return SetSocketOptionValue(sock, option, reinterpret_cast<const char*>(value), sizeof(value));
        }

        //=========================================================================
        // EnableTCPNoDelay
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode EnableTCPNoDelay(SocketDriverCommon::SocketType sock, bool enable)
        {
            AZ::u32 val = enable ? 1 : 0;
            if (IsSocketError(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&val), sizeof(val))))
            {
                return Driver::EC_SOCKET_SOCK_OPT;
            }
            return Driver::EC_OK;
        }

        //=========================================================================
        // SetSocketBlockingMode
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode SetSocketBlockingMode(SocketDriverCommon::SocketType sock, bool blocking)
        {
#if   defined(AZ_PLATFORM_ANDROID) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_LINUX)
            AZ::s64 result = -1;
            AZ::s32 flags = ::fcntl(sock, F_GETFL);
            flags &= ~O_NONBLOCK;
            flags |= (blocking ? 0 : O_NONBLOCK);
            result = ::fcntl(sock, F_SETFL, flags);
#elif defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_X360) || defined(AZ_PLATFORM_XBONE)
            AZ::s64 result = SOCKET_ERROR;
            u_long val = blocking ? 0 : 1;
            result = ioctlsocket(sock, FIONBIO, &val);
#else
#error Platform not supported!
#endif
            if (IsSocketError(result))
            {
                return Driver::EC_SOCKET_MAKE_NONBLOCK;
            }
            return Driver::EC_OK;
        }

        //=========================================================================
        // SetSocketLingerTime
        // [5/4/2017]
        //=========================================================================
        GridMate::Driver::ResultCode SetSocketLingerTime(SocketDriverCommon::SocketType sock, bool bDoLinger, AZ::u16 timeout)
        {
            // Indicates the state of the linger structure associated with a socket. If the l_onoff member of the linger 
            // structure is nonzero, a socket remains open for a specified amount of time after a closesocket function call to 
            // enable queued data to be sent. The amount of time, in seconds, to remain open is specified in the l_linger member 
            // of the linger structure. This option is only valid for reliable, connection - oriented protocols.
            struct linger theLinger;
            theLinger.l_linger = static_cast<decltype(theLinger.l_linger)>(timeout);
            theLinger.l_onoff = static_cast<decltype(theLinger.l_onoff)>(bDoLinger);
            AZ::s64 sockResult = setsockopt(sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&theLinger), sizeof(theLinger));
            AZ_Error("GridMate", sockResult == 0, "Socket option: %d failed to set. Error:%d", SO_LINGER, GetSocketError());
            return (sockResult == 0) ? Driver::EC_OK : Driver::EC_SOCKET_SOCK_OPT;
        }

        //=========================================================================
        // CloseSocket
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode CloseSocket(SocketDriverCommon::SocketType sock)
        {
            if (IsValidSocket(sock))
            {
                if (!IsSocketError(closesocket(sock)))
                {
                    return Driver::EC_OK;
                }
            }
            return Driver::EC_SOCKET_CLOSE;
        }

        //=========================================================================
        // Send
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode Send(SocketDriverCommon::SocketType sock, const char* buf, AZ::u32 bufLen, AZ::u32& bytesSent)
        {
            bytesSent = 0;
            if (bufLen == 0)
            {
                // is an empty buffer?
                return Driver::EC_SEND;
            }
            else if ((0x80000000 & bufLen) != 0)
            {
                // is negative?
                return Driver::EC_SEND;
            }
#if   defined(AZ_PLATFORM_ANDROID) || defined(AZ_PLATFORM_LINUX)
            AZ::s32 msgNoSignal = MSG_NOSIGNAL;
#elif defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
            AZ::s32 msgNoSignal = 0;
#endif
            AZ::s32 result = send(sock, buf, static_cast<AZ::u32>(bufLen), msgNoSignal);
            if (IsSocketError(result))
            {
                AZ::s32 err = GetSocketError();
                if (err != AZ_EWOULDBLOCK)
                {
                    AZ_TracePrintf("GridMate", "send() err:%d -> %s\n", err, AZ::AzSock::GetStringForError(err));
                    return Driver::EC_SEND;
                }
            }
            else
            {
                bytesSent = result;
            }
            return Driver::EC_OK;
        }

        //=========================================================================
        // Receive
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode Receive(SocketDriverCommon::SocketType sock, char* buf, AZ::u32& inOutlen)
        {
            if (inOutlen == 0)
            {
                // is an empty buffer?
                return Driver::EC_RECEIVE;
            }
            else if ((0x80000000 & inOutlen) != 0)
            {
                // is negative?
                return Driver::EC_RECEIVE;
            }
            AZ::s32 result = recv(sock, buf, static_cast<AZ::u32>(inOutlen), 0);
            if (IsSocketError(result))
            {
                inOutlen = 0;
                AZ::s32 err = GetSocketError();
                if (err != AZ_EWOULDBLOCK)
                {
                    AZ_TracePrintf("GridMate", "recv() err:%d -> %s\n", err, AZ::AzSock::GetStringForError(err));
                    return Driver::EC_RECEIVE;
                }
            }
            inOutlen = result;
            return Driver::EC_OK;
        }

        //=========================================================================
        // Bind
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode Bind(SocketDriverCommon::SocketType sock, const sockaddr* sockAddr, size_t sockAddrSize)
        {
            AZ::s32 ret = bind(sock, sockAddr, static_cast<socklen_t>(sockAddrSize));
            if (IsSocketError(ret))
            {
                return Driver::EC_SOCKET_BIND;
            }
            return Driver::EC_OK;
        }

        //=========================================================================
        // Connect
        // [3/23/2017]
        //=========================================================================
        GridMate::Driver::ResultCode Connect(SocketDriverCommon::SocketType sock, const sockaddr* socketAddress, size_t sockAddrSize, ConnectionResult& outConnectionResult)
        {
            socklen_t addressSize = static_cast<socklen_t>(sockAddrSize);
            AZ::s64 err = connect(sock, socketAddress, addressSize);
            if (!IsSocketError(err))
            {
                outConnectionResult = ConnectionResult::Okay;
                return Driver::EC_OK;
            }
            else
            {
                // okay for non-blocking sockets... will take a while
                AZ::s64 extendedErr = GetSocketError();
                if (extendedErr == static_cast<AZ::s32>(AZ_EWOULDBLOCK))
                {
                    outConnectionResult = ConnectionResult::InProgress;
                    return Driver::EC_OK;
                }
                else if (extendedErr == static_cast<AZ::s32>(AZ_EINPROGRESS))
                {
                    outConnectionResult = ConnectionResult::InProgress;
                    return Driver::EC_OK;
                }
                else if (extendedErr == static_cast<AZ::s32>(AZ_EALREADY))
                {
                    outConnectionResult = ConnectionResult::InProgress;
                    return Driver::EC_OK;
                }
                else if (extendedErr == static_cast<AZ::s32>(AZ_ECONNREFUSED))
                {
                    outConnectionResult = ConnectionResult::Refused;
                }
                else if (extendedErr == static_cast<AZ::s32>(AZ_EISCONN))
                {
                    outConnectionResult = ConnectionResult::SocketConnected;
                }
                else if (extendedErr == static_cast<AZ::s32>(AZ_ENETUNREACH))
                {
                    outConnectionResult = ConnectionResult::NetworkUnreachable;
                }
                else if (extendedErr == static_cast<AZ::s32>(AZ_ETIMEDOUT))
                {
                    outConnectionResult = ConnectionResult::TimedOut;
                }
            }
            AZ_TracePrintf("GridMate", "Connect() error:%d\n", GetSocketError());
            return Driver::EC_SOCKET_CONNECT;
        }

        //=========================================================================
        // Connect
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode Connect(SocketDriverCommon::SocketType sock, const SocketDriverAddress& addr, ConnectionResult& outConnectionResult)
        {
            AZ::u32 addressSize;
            const sockaddr* socketAddress = reinterpret_cast<const sockaddr*>(addr.GetTargetAddress(addressSize));
            if (socketAddress == nullptr)
            {
                return Driver::EC_SOCKET_CONNECT;
            }
            return Connect(sock, socketAddress, addressSize, outConnectionResult);
        }

        //=========================================================================
        // Listen
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode Listen(SocketDriverCommon::SocketType sock, AZ::s32 backlog)
        {
            if (IsSocketError(listen(sock, backlog)))
            {
                return Driver::EC_SOCKET_LISTEN;
            }
            return Driver::EC_OK;
        }

        //=========================================================================
        // Accept
        // [3/23/2017]
        //=========================================================================
        Driver::ResultCode Accept(SocketDriverCommon::SocketType sock, sockaddr* outAddr, socklen_t& outAddrSize, SocketDriverCommon::SocketType& outSocket)
        {
            if (outAddrSize < static_cast<socklen_t>(sizeof(sockaddr_in)))
            {
                return Driver::EC_SOCKET_ACCEPT;
            }
            memset(outAddr, 0, outAddrSize);

            outSocket = accept(sock, outAddr, &outAddrSize);
            if (!IsValidSocket(outSocket))
            {
                // okay for non-blocking sockets... will take a while
                AZ::s64 extendedErr = GetSocketError();
                if (extendedErr == static_cast<AZ::s32>(AZ_EWOULDBLOCK))
                {
                    return Driver::EC_OK;
                }
                else if (extendedErr == static_cast<AZ::s32>(AZ_EINPROGRESS))
                {
                    return Driver::EC_OK;
                }
                return Driver::EC_SOCKET_ACCEPT;
            }
            return Driver::EC_OK;
        }

        //=========================================================================
        // GetTimeValue
        // [3/23/2017]
        //=========================================================================
        timeval GetTimeValue(AZStd::chrono::microseconds timeOut)
        {
            timeval t;
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
            t.tv_sec = static_cast<long>(timeOut.count() / 1000000);
            t.tv_usec = static_cast<long>(timeOut.count() % 1000000);
#else
            t.tv_sec = static_cast<time_t>(timeOut.count() / 1000000);
            t.tv_usec = static_cast<suseconds_t>(timeOut.count() % 1000000);
#endif
            return t;
        }

        //=========================================================================
        // IsWritable
        // [3/23/2017]
        //=========================================================================
        bool IsWritable(SocketDriverCommon::SocketType sock, AZStd::chrono::microseconds timeOut)
        {
            fd_set fdwrite;
            FD_ZERO(&fdwrite);
            FD_SET(sock, &fdwrite);
            timeval t = GetTimeValue(timeOut);
            int result = select(FD_SETSIZE, nullptr, &fdwrite, nullptr, &t);
            if (result > 0)
            {
                return true;
            }
            AZ_Warning("GridMate", result == 0, "Socket:%d select error %d\n", sock, GetSocketError());
            return false;
        }

        //=========================================================================
        // IsReceivePending
        // [3/23/2017]
        //=========================================================================
        bool IsReceivePending(SocketDriverCommon::SocketType sock, AZStd::chrono::microseconds timeOut)
        {
            fd_set fdread;
            FD_ZERO(&fdread);
            FD_SET(sock, &fdread);
            timeval t = GetTimeValue(timeOut);
            int result = select(FD_SETSIZE, &fdread, nullptr, nullptr, &t);
            if (result > 0)
            {
                return true;
            }
            AZ_Warning("GridMate", result == 0, "Socket:%d select error %d\n", sock, GetSocketError());
            return false;
        }
    }

    AZStd::size_t SocketDriverAddress::Hasher::operator()(const SocketDriverAddress& v) const
    {
        size_t hash = 0;
        switch (v.m_sockAddr.sin_family)
        {
            case AF_INET:
            {
                hash = v.m_sockAddr.sin_addr.s_addr ^ v.m_sockAddr.sin_port;
                break;
            }
            case AF_INET6:
            {
                union
                {
                    const void* m_voidPtr;
                    const size_t* m_hashPtr;
                } u;
                u.m_voidPtr = static_cast<const void*>(v.m_sockAddr6.sin6_addr.s6_addr);
                hash = *u.m_hashPtr ^ v.m_sockAddr6.sin6_port;
                break;
            }
        }
        return hash;
    }

    SocketDriverAddress::SocketDriverAddress()
        : DriverAddress(nullptr)
    {
        m_sockAddr.sin_family = AF_UNSPEC;
        m_sockAddr.sin_port = 0;
        m_sockAddr.sin_addr.s_addr = 0;
    }

    SocketDriverAddress::SocketDriverAddress(Driver* driver)
        : DriverAddress(driver)
    {
        m_sockAddr.sin_family = AF_UNSPEC;
        m_sockAddr.sin_port = 0;
        m_sockAddr.sin_addr.s_addr = 0;
    }

    SocketDriverAddress::SocketDriverAddress(Driver* driver, const sockaddr* addr)
        : DriverAddress(driver)
    {
        if (addr->sa_family == AF_INET6)
        {
            m_sockAddr6 = *reinterpret_cast<const sockaddr_in6*>(addr);
        }
        else
        {
            m_sockAddr = *reinterpret_cast<const sockaddr_in*>(addr);
        }
    }

    SocketDriverAddress::SocketDriverAddress(Driver* driver, const string& ip, unsigned int port)
        : DriverAddress(driver)
    {
        AZ_Assert(!ip.empty(), "Invalid address string!");
        // resolve address
        {
            addrinfo  hints;
            memset(&hints, 0, sizeof(struct addrinfo));
            addrinfo* addrInfo;
            hints.ai_family = AF_UNSPEC;
            hints.ai_flags = AI_CANONNAME;
            char strPort[8];
            azsnprintf(strPort, AZ_ARRAY_SIZE(strPort), "%d", port); // TODO pass port as a string to avoid this call

            const char* address = ip.c_str();

            if (address && strlen(address) == 0) // getaddrinfo doesn't accept empty string
            {
                address = nullptr;
            }

            int error = getaddrinfo(address, strPort, &hints, &addrInfo);
            if (error == 0)
            {
                if (addrInfo->ai_family == AF_INET)
                {
                    m_sockAddr = *reinterpret_cast<const sockaddr_in*>(addrInfo->ai_addr);
                }
                else if (addrInfo->ai_family == AF_INET6)
                {
                    m_sockAddr6 = *reinterpret_cast<const sockaddr_in6*>(addrInfo->ai_addr);
                }

                freeaddrinfo(addrInfo);
            }
            else
            {
#if defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_ANDROID)
                AZ_Assert(false, "SocketDriver::ResolveAddress failed '%s'!", error == EAI_SYSTEM ? strerror(GetSocketError()) : gai_strerror(error));
#else
                AZ_Assert(false, "SocketDriver::ResolveAddress failed with error %d!", GetSocketError());
#endif
            }
        }
    }

    bool SocketDriverAddress::operator==(const SocketDriverAddress& rhs) const
    {
        if (m_sockAddr.sin_family != rhs.m_sockAddr.sin_family)
        {
            return false;
        }

        if (m_sockAddr.sin_family == AF_INET6)
        {
            if (m_sockAddr6.sin6_port != rhs.m_sockAddr6.sin6_port)
            {
                return false;
            }

            const AZ::u64* addr = reinterpret_cast<const AZ::u64*>(m_sockAddr6.sin6_addr.s6_addr);
            const AZ::u64* rhsAddr = reinterpret_cast<const AZ::u64*>(rhs.m_sockAddr6.sin6_addr.s6_addr);
            if (addr[0] != rhsAddr[0] || addr[1] != rhsAddr[1])
            {
                return false;
            }
        }
        else
        {
            return m_sockAddr.sin_addr.s_addr == rhs.m_sockAddr.sin_addr.s_addr && m_sockAddr.sin_port == rhs.m_sockAddr.sin_port;
        }
        return true;
    }

    bool SocketDriverAddress::operator!=(const SocketDriverAddress& rhs) const
    {
        return !(*this == rhs);
    }

    string SocketDriverAddress::ToString() const
    {
        char ip[64];
        unsigned short port;
        if (m_sockAddr.sin_family == AF_INET6)
        {
            inet_ntop(AF_INET6, const_cast<in6_addr*>(&m_sockAddr6.sin6_addr), ip, AZ_ARRAY_SIZE(ip));
            port = ntohs(m_sockAddr6.sin6_port);
        }
        else
        {
            inet_ntop(AF_INET, const_cast<in_addr*>(&m_sockAddr.sin_addr), ip, AZ_ARRAY_SIZE(ip));
            port = ntohs(m_sockAddr.sin_port);
        }

        return string::format("%s|%d", ip, port);
    }

    string SocketDriverAddress::ToAddress() const
    {
        return ToString();
    }

    string SocketDriverAddress::GetIP() const
    {
        char ip[64];
        if (m_sockAddr.sin_family == AF_INET6)
        {
            inet_ntop(AF_INET6, const_cast<void*>(reinterpret_cast<const void*>(&m_sockAddr6.sin6_addr)), ip, AZ_ARRAY_SIZE(ip));
        }
        else
        {
            inet_ntop(AF_INET, const_cast<void*>(reinterpret_cast<const void*>(&m_sockAddr.sin_addr)), ip, AZ_ARRAY_SIZE(ip));
        }
        return string(ip);
    }

    unsigned int SocketDriverAddress::GetPort() const
    {
        if (m_sockAddr.sin_family == AF_INET6)
        {
            return ntohs(m_sockAddr6.sin6_port);
        }
        else
        {
            return ntohs(m_sockAddr.sin_port);
        }
    }

    const void* SocketDriverAddress::GetTargetAddress(unsigned int& addressSize) const
    {
        if (m_sockAddr.sin_family == AF_INET6)
        {
            addressSize = sizeof(m_sockAddr6);
        }
        else
        {
            addressSize = sizeof(m_sockAddr);
        }
        return &m_sockAddr;
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // SocketAddressInfo
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    SocketAddressInfo::SocketAddressInfo()
        : m_addrInfo(nullptr)
    {
    }

    SocketAddressInfo::~SocketAddressInfo()
    {
        Reset();
    }

    void SocketAddressInfo::Reset()
    {
        if (m_addrInfo != nullptr)
        {
            freeaddrinfo(m_addrInfo);
            m_addrInfo = nullptr;
        }
    }
    bool SocketAddressInfo::Resolve(char const* address, AZ::u16 port, Driver::BSDSocketFamilyType familyType, bool isDatagram, AdditionalOptionFlags flags)
    {
        AZ_Assert(familyType == Driver::BSDSocketFamilyType::BSD_AF_INET || familyType == Driver::BSDSocketFamilyType::BSD_AF_INET6, "Family type (familyType) can be IPV4 or IPV6 only!");
        Reset();

        char portStr[8];
        azsnprintf(portStr, AZ_ARRAY_SIZE(portStr), "%d", port);

        addrinfo  hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = (familyType == Driver::BSDSocketFamilyType::BSD_AF_INET6) ? AF_INET6 : AF_INET;
        hints.ai_socktype = isDatagram ? SOCK_DGRAM : SOCK_STREAM;
        hints.ai_flags = 0;

        AZ::u32 dwFlags = static_cast<AZ::u32>(flags);
        if ((dwFlags & static_cast<AZ::u32>(AdditionalOptionFlags::Passive)) == static_cast<AZ::u32>(AdditionalOptionFlags::Passive))
        {
            hints.ai_flags |= AI_PASSIVE;
        }
        if ((dwFlags & static_cast<AZ::u32>(AdditionalOptionFlags::NumericHost)) == static_cast<AZ::u32>(AdditionalOptionFlags::NumericHost))
        {
            hints.ai_flags |= AI_NUMERICHOST;
        }

        if (address && strlen(address) == 0)
        {
            address = nullptr;
        }

        int error = getaddrinfo(address, portStr, &hints, &m_addrInfo);
        if (error != 0)
        {
#if defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_ANDROID) // \todo We need to revisit the socket interface, too many defines
            AZ_TracePrintf("GridMate", "SocketDriver::Initialize - getaddrinfo failed with %s!\n", error == EAI_SYSTEM ? strerror(GetSocketError()) : gai_strerror(error));
#else
            AZ_TracePrintf("GridMate", "SocketDriver::Initialize - getaddrinfo failed with code %d at port %d\n", GetSocketError(), port);
#endif
            return false;
        }
        return true;
    }

    AZ::u16 SocketAddressInfo::RetrieveSystemAssignedPort(SocketDriverCommon::SocketType socket) const
    {
        if (m_addrInfo == nullptr)
        {
            return 0;
        }
        socklen_t addrLen = static_cast<socklen_t>(m_addrInfo->ai_addrlen);
        if (getsockname(socket, m_addrInfo->ai_addr, &addrLen) == 0)
        {
            if (addrLen == sizeof(sockaddr_in6))
            {
                return reinterpret_cast<sockaddr_in6*>(m_addrInfo->ai_addr)->sin6_port;
            }
            else
            {
                return reinterpret_cast<sockaddr_in*>(m_addrInfo->ai_addr)->sin_port;
            }
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // SocketDriverCommon
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    //=========================================================================
    // SocketDriverCommon
    // [10/14/2010]
    //=========================================================================
    SocketDriverCommon::SocketDriverCommon(bool isFullPackets, bool isCrossPlatform)
        : m_isFullPackets(isFullPackets)
        , m_isCrossPlatform(isCrossPlatform)
        , m_isIpv6(false)
        , m_isDatagram(true)
    {
        m_port = 0;
        m_isStoppedWaitForData = false;
    }

    //=========================================================================
    // ~SocketDriverCommon
    // [10/14/2010]
    //=========================================================================
    SocketDriverCommon::~SocketDriverCommon()
    {
        if (IsValidSocket(m_socket))
        {
            //shutdown(m_socket,SD_BOTH); XBone extra line ??? Test
            closesocket(m_socket);
            m_socket = GetInvalidSocket();
        }
    }

    //=========================================================================
    // GetMaxSendSize
    // [1/7/2011]
    //=========================================================================
    unsigned int
    SocketDriverCommon::GetMaxSendSize() const
    {

        if (m_isFullPackets)
        {
            return m_isCrossPlatform ? 1264 : 65507; // X360 has the lowest cross platform so respect it, other wise max for UDP
        }
        else
        {
            return m_isCrossPlatform ? 1264 : 1400; // max for x360 if cross platform, otherwise max for UDP
        }

    }

    //=========================================================================
    // GetMaxSendSize
    // [1/7/2011]
    //=========================================================================
    unsigned int
    SocketDriverCommon::GetPacketOverheadSize() const
    {
    #if defined(AZ_PLATFORM_X360)
        if (!m_isFullPackets)
        {
            /*
            X360
            UDP/VDP has a 44 byte header when using port 1000 - the header is up to 4 bytes larger when using other ports
            using voice chat over VDP adds an additional 2 bytes to the packet header
            worst case VDP header with voice is 52 bytes - additional overhead incurred for using other ports cannot exceed 4 bytes so this is partially unaccounted for
            */
            return 52;
        }
        else
    #endif
        {
            return 8 /* standard UDP*/ + 20 /* min for IPv4 */;
        }
    }

    //=========================================================================
    // CreateSocket
    // [2/25/2011]
    //=========================================================================
    SocketDriverCommon::SocketType
    SocketDriverCommon::CreateSocket(int af, int type, int protocol)
    {
        return socket(af, type, protocol);
    }

    //=========================================================================
    // BindSocket
    // [2/25/2011]
    //=========================================================================
    int
    SocketDriverCommon::BindSocket(const sockaddr* sockAddr, size_t sockAddrSize)
    {
        return bind(m_socket, sockAddr, static_cast<socklen_t>(sockAddrSize));
    }

    //=========================================================================
    // SetSocketOptions
    // [2/25/2011]
    //=========================================================================
    Driver::ResultCode
    SocketDriverCommon::SetSocketOptions(bool isBroadcast, unsigned int receiveBufferSize, unsigned int sendBufferSize)
    {
        int sock_opt;
        // set nonblocking
        unsigned long sock_ctrl = 1;
        int sockResult = 0;
        (void)sockResult;
        if (IsSocketError(ioctlsocket(m_socket, SO_NBIO, &sock_ctrl)))
        {
            closesocket(m_socket);
            int error = GetSocketError();
            (void)error;
            AZ_TracePrintf("GridMate", "SocketDriver::Initialize - ioctlsocket failed with code %d\n", error);
            return EC_SOCKET_MAKE_NONBLOCK;
        }

        // receive buffer size
        sock_opt = receiveBufferSize == 0 ? (1024 * 256) : receiveBufferSize;
        sockResult = setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&sock_opt), sizeof(sock_opt));
        AZ_Error("GridMate", sockResult == 0, "Failed to set receive buffer to %d size. Error: %d", sock_opt, GetSocketError());

        // send buffer size
        sock_opt = sendBufferSize == 0 ? (1024 * 64) : sendBufferSize;
        sockResult = setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sock_opt), sizeof(sock_opt));
        AZ_Error("GridMate", sockResult == 0, "Failed to set send buffer to %d size. Error: %d", sock_opt, GetSocketError());

        // make sure we allow both ipv4 and ipv6 (we can make this optional)
        if (m_isIpv6)
        {
            sock_opt = 0;
            sockResult = setsockopt(m_socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&sock_opt), sizeof(sock_opt));
            AZ_Error("GridMate", sockResult == 0, "Failed to stop using ipv6 only. Error: %d", GetSocketError());

    #if !defined(AZ_PLATFORM_XBONE) && !defined(AZ_PLATFORM_APPLE) // XBone doesn't allow multicast. Apple is TODO
            // we emulate broadcast support over ipv6 (todo enable multicast support with an address too)
            addrinfo hints;
            memset(&hints, 0, sizeof(hints));
            addrinfo* multicastInfo;
            hints.ai_family = AF_INET6;
            hints.ai_flags = AI_NUMERICHOST;
            sockResult = getaddrinfo(Utils::GetBroadcastAddress(BSD_AF_INET6), nullptr, &hints, &multicastInfo);
            AZ_Error("GridMate", sockResult == 0, "getaddrinfo failed to get broadcast address. Error: %d", GetSocketError());

            ipv6_mreq multicastRequest;
            multicastRequest.ipv6mr_interface = 0;
            memcpy(&multicastRequest.ipv6mr_multiaddr, &reinterpret_cast<sockaddr_in6*>(multicastInfo->ai_addr)->sin6_addr, sizeof(multicastRequest.ipv6mr_multiaddr));

            freeaddrinfo(multicastInfo);

            if (m_isDatagram)
            {
                sockResult = setsockopt(m_socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, reinterpret_cast<char*>(&multicastRequest), sizeof(multicastRequest));

                AZ_Error("GridMate", sockResult == 0, "Failed to IPV6_ADD_MEMBERSHIP. Error: %d", GetSocketError());
            }

    #endif // !AZ_PLATFORM_XBONE
        }
        else
        {
            if (isBroadcast)
            {
                sock_opt = 1;
                sockResult = setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&sock_opt), sizeof(sock_opt));
                AZ_Error("GridMate", sockResult == 0, "Failed to enable broadcast. Error: %d", GetSocketError());
            }
        }

    #if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
        // faster socket close
        linger l;
        l.l_onoff = 0;
        l.l_linger = 0;
        setsockopt(m_socket, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&l), sizeof(l));

        // TODO: Remove this when when we don't support windows XP! (if fixed later)
        // http://support.microsoft.com/?kbid=263823
        // WinSock Recvfrom() now returns WSAECONNRESET instead of blocking or timing out

        // MS Transport Provider IOCTL to control
        // reporting PORT_UNREACHABLE messages
        // on UDP sockets via recv/WSARecv/etc.
        // Path TRUE in input buffer to enable (default if supported),
        // FALSE to disable.
    #define SIO_UDP_CONNRESET   _WSAIOW(IOC_VENDOR, 12)

        if (m_isDatagram)
        {
            // disable  new behavior using
            // IOCTL: SIO_UDP_CONNRESET
            DWORD dwBytesReturned = 0;
            BOOL isReportPortUnreachable = FALSE;
            if (WSAIoctl(m_socket, SIO_UDP_CONNRESET, &isReportPortUnreachable, sizeof(isReportPortUnreachable), NULL, 0, &dwBytesReturned, NULL, NULL) == SOCKET_ERROR)
            {
                closesocket(m_socket);
                int error = GetSocketError();
                (void)error;
                AZ_TracePrintf("GridMate", "SocketDriver::Initialize - WSAIoctl failed with code %d\n", error);
                return EC_SOCKET_SOCK_OPT;
            }
        }

    #endif // AZ_PLATFORM_WINDOWS

        return EC_OK;
    }

    //=========================================================================
    // Initialize
    // [10/14/2010]
    //=========================================================================
    Driver::ResultCode
    SocketDriverCommon::Initialize(int ft, const char* address, unsigned int port, bool isBroadcast, unsigned int receiveBufferSize, unsigned int sendBufferSize)
    {
        AZ_Assert(ft == BSD_AF_INET || ft == BSD_AF_INET6, "Family type (ft) can be IPV4 or IPV6 only!");
        m_isIpv6 = (ft == BSD_AF_INET6);

        m_port = htons(static_cast<unsigned short>(port));
        char portStr[8];
        azsnprintf(portStr, AZ_ARRAY_SIZE(portStr), "%d", port);

        addrinfo* addrInfo;
        addrinfo  hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = m_isIpv6 ? AF_INET6 : AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        if (address && strlen(address) == 0)
        {
            address = nullptr;
        }

        int error = getaddrinfo(address, portStr, &hints, &addrInfo);
        if (error != 0)
        {
    #if defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_ANDROID) // \todo We need to revisit the socket interface, too many defines
            AZ_TracePrintf("GridMate", "SocketDriver::Initialize - getaddrinfo failed with %s!\n", error == EAI_SYSTEM ? strerror(GetSocketError()) : gai_strerror(error));
    #else
            AZ_TracePrintf("GridMate", "SocketDriver::Initialize - getaddrinfo failed with code %d at port %d\n", GetSocketError(), port);
    #endif
            return EC_SOCKET_CREATE;
        }

        m_socket = CreateSocket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);

        if (IsValidSocket(m_socket))
        {
            ResultCode res = SetSocketOptions(isBroadcast, receiveBufferSize, sendBufferSize);
            if (res != EC_OK)
            {
                freeaddrinfo(addrInfo);
                closesocket(m_socket);
                return res;
            }

            if (IsSocketError(BindSocket(addrInfo->ai_addr, addrInfo->ai_addrlen)))
            {
                int socketErr = GetSocketError();
                (void)socketErr;
                AZ_TracePrintf("GridMate", "SocketDriver::Initialize - bind failed with code %d at port %d\n", socketErr, port);
                freeaddrinfo(addrInfo);
                closesocket(m_socket);
                return EC_SOCKET_BIND;
            }

            if (m_port == 0)  // if we use implicit bind, retrieve the system assigned port
            {
                socklen_t addrLen = static_cast<socklen_t>(addrInfo->ai_addrlen);
                if (getsockname(m_socket, addrInfo->ai_addr, &addrLen) == 0)
                {
                    if (addrLen == sizeof(sockaddr_in6))
                    {
                        m_port = reinterpret_cast<sockaddr_in6*>(addrInfo->ai_addr)->sin6_port;
                    }
                    else
                    {
                        m_port = reinterpret_cast<sockaddr_in*>(addrInfo->ai_addr)->sin_port;
                    }
                }

                AZ_Error("GridMate", m_port != 0, "Failed to implicitly assign port (getsockname faled with %d)!", GetSocketError());
                if (m_port == 0)
                {
                    int socketErr = GetSocketError();
                    (void)socketErr;
                    AZ_TracePrintf("GridMate", "SocketDriver::Initialize - getsockname failed with code %d at port %d\n", socketErr, port);
                    freeaddrinfo(addrInfo);
                    closesocket(m_socket);
                    return EC_SOCKET_BIND;
                }
            }
        }
        else
        {
            m_port = 0;
            int socketErr = GetSocketError();
            (void)socketErr;
            AZ_TracePrintf("GridMate", "SocketDriver::Initialize - socket failed with code %d at port %d\n", socketErr, port);
            freeaddrinfo(addrInfo);
            return EC_SOCKET_CREATE;
        }
        freeaddrinfo(addrInfo);
        return EC_OK;
    }

    //=========================================================================
    // GetPort
    // [7/9/2012]
    //=========================================================================
    unsigned int
    SocketDriverCommon::GetPort() const
    {
        return ntohs(m_port);
    }

    //=========================================================================
    // Send
    // [10/14/2010]
    //=========================================================================
    Driver::ResultCode
    SocketDriverCommon::Send(const AZStd::intrusive_ptr<DriverAddress>& to, const char* data, unsigned int dataSize)
    {
        AZ_Assert(to && data, "Invalid function input!");
        AZ_Assert(dataSize <= SocketDriverCommon::GetMaxSendSize(), "Size is too big to send! Must be less than %d bytes", SocketDriverCommon::GetMaxSendSize());

        unsigned int addressSize;
        const sockaddr* sockAddr = reinterpret_cast<const sockaddr*>(to->GetTargetAddress(addressSize));
        //AZ_TracePrintf("GridMate","%p Sending %d bytes to %s\n",this,dataSize, to->ToString().c_str());
        if (sockAddr == nullptr)
        {
    #ifdef AZ_LOG_UNBOUND_SEND_RECEIVE
            AZ_TracePrintf("GridMate", "SocketDriver::Send - address %s is not bound. This is not an error if you support unbound connectins, but data was NOT send!\n", to->ToString().c_str());
    #endif // AZ_LOG_UNBOUND_SEND_RECEIVE
            return /*EC_SEND_ADDRESS_NOT_BOUND*/ EC_OK;
        }

        do
        {
            if (IsSocketError(sendto(m_socket, data, dataSize, 0, sockAddr, static_cast<socklen_t>(addressSize))))
            {
                int errorCode = GetSocketError();
                int wouldNotBlock = AZ_EWOULDBLOCK;
                // if a non blocking socket (todo add a quick check)
                if (errorCode != wouldNotBlock)   // it's ok if a non blocking socket can't get the command instantly
                {
                    AZ_Error("GridMate", false, "SocketDriver::Send - sendto failed with code %d at address %s!", errorCode, to->ToString().c_str());
                }
                else
                {
                    // If we run out of buffer just wait for some buffer to become available
                    fd_set fdwrite;
                    FD_ZERO(&fdwrite);
                    FD_SET(m_socket, &fdwrite);
                    select(FD_SETSIZE, 0, &fdwrite, 0, 0);
                    continue;
                }

                return EC_SEND;
            }
            else
            {
                break;
            }
        } while (true);
        return EC_OK;
    }

    //=========================================================================
    // Receive
    // [10/14/2010]
    //=========================================================================
    unsigned int
    SocketDriverCommon::Receive(char* data, unsigned int maxDataSize, AZStd::intrusive_ptr<DriverAddress>& from, ResultCode* resultCode)
    {
        AZ_Assert(data, "Invalid function input!");
        //AZ_Assert(maxDataSize>=GetMaxSendSize(),"Size is too small to receive data! Must be atleast %d bytes",GetMaxSendSize());
        union // or use sockaddr_storage
        {
            sockaddr_in  sockAddrIn;
            sockaddr_in6 sockAddrIn6;
        };
        sockaddr* sockAddr = reinterpret_cast<sockaddr*>(&sockAddrIn6);
        socklen_t sockAddrLen = sizeof(sockAddrIn6);
        from = NULL;
        AZ::s64 recvd;
        do
        {
            recvd = recvfrom(m_socket, data, maxDataSize, 0, sockAddr, &sockAddrLen);
            if (IsSocketError(recvd))
            {
                int error = GetSocketError();

                int wouldNotBlock = AZ_EWOULDBLOCK;

                // if a non blocking socket (todo add a quick check)
                if (error == wouldNotBlock)
                {
                    if (resultCode)
                    {
                        *resultCode = EC_OK;
                    }
                    return 0;  // this is normal for non blocking sockets
                }

                (void)error;
                AZ_TracePrintf("GridMate", "SocketDriver::Receive - recvfrom failed with code %d, dataSize=%d\n", error, maxDataSize);
                if (resultCode)
                {
                    *resultCode = EC_RECEIVE;
                }
                return 0;
            }

            if (recvd != sizeof(AZ_SOCKET_WAKEUP_MSG_TYPE) || *(AZ_SOCKET_WAKEUP_MSG_TYPE*)data != AZ_SOCKET_WAKEUP_MSG_VALUE)
            {
                break;  // internal wake up message
            }
        } while (true);

        if (recvd)
        {
            from = CreateDriverAddress(sockAddr);
            if (!from)  // if we did not assign an address, ignore the data.
            {
                recvd = 0;
    #ifdef AZ_LOG_UNBOUND_SEND_RECEIVE
                char ip[64];
                unsigned short port;
                if (sockAddrLen == sizeof(sockAddrIn6))
                {
                    inet_ntop(AF_INET6, &sockAddrIn6.sin6_addr, ip, AZ_ARRAY_SIZE(ip));
                    port = htons(sockAddrIn6.sin6_port);
                }
                else
                {
                    inet_ntop(AF_INET, &sockAddrIn.sin_addr, ip, AZ_ARRAY_SIZE(ip));
                    port = htons(sockAddrIn.sin_port);
                }
                (void)port;
                AZ_TracePrintf("GridMate", "Data discarded from %s|%d\n", ip, port);
    #endif // AZ_LOG_UNBOUND_SEND_RECEIVE
            }
            /*else
            {
            AZ_TracePrintf("GridMate","%p Received %d bytes from %s\n",this,recvd,from->ToString().c_str());
            }*/
        }

        if (resultCode)
        {
            *resultCode = EC_OK;
        }

        return static_cast<unsigned int>(recvd);
    }

    //=========================================================================
    // WaitForData
    // [6/5/2012]
    //=========================================================================
    bool
    SocketDriverCommon::WaitForData(AZStd::chrono::microseconds timeOut)
    {
        // If we run out of buffer just wait for some buffer to become available
        fd_set fdread;
        FD_ZERO(&fdread);
        FD_SET(m_socket, &fdread);
        timeval t;
    #if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
        t.tv_sec = static_cast<long>(timeOut.count() / 1000000);
        t.tv_usec = static_cast<long>(timeOut.count() % 1000000);
    #else
        t.tv_sec = static_cast<time_t>(timeOut.count() / 1000000);
        t.tv_usec = static_cast<suseconds_t>(timeOut.count() % 1000000);
    #endif
        int result = select(FD_SETSIZE, &fdread, 0, 0, &t);
        if (result > 0)
        {
            m_isStoppedWaitForData = true;
            return true;
        }
        AZ_Warning("GridMate", result >= 0, "Socket select error %d\n", GetSocketError());
        m_isStoppedWaitForData = false;
        return false;
    }

    //=========================================================================
    // StopWaitForData
    // [6/5/2012]
    //=========================================================================
    void
    SocketDriverCommon::StopWaitForData()
    {
        // This is a little tricky we just send one byte of data on a loopback
        // so we unlock the select function. Data will be discarded.
        if (IsValidSocket(m_socket))
        {
            const AZ_SOCKET_WAKEUP_MSG_TYPE data = AZ_SOCKET_WAKEUP_MSG_VALUE;
            if (m_isIpv6)
            {
                sockaddr_in6 sockAddr;
                sockAddr.sin6_family = AF_INET6;
                sockAddr.sin6_addr = in6addr_loopback;
                sockAddr.sin6_port = m_port;
                sendto(m_socket, (const char*)&data, sizeof(data), 0, (const sockaddr*)&sockAddr, sizeof(sockAddr)); // if an error occurs we don't care as we will wake up anyway
            }
            else
            {
                sockaddr_in sockAddr;
                sockAddr.sin_family = AF_INET;
                sockAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                sockAddr.sin_port = m_port;
                sendto(m_socket, (const char*)&data, sizeof(data), 0, (const sockaddr*)&sockAddr, sizeof(sockAddr)); // if an error occurs we don't care as we will wake up anyway
            }
        }
    }

    //=========================================================================
    // CreateSocketDriver
    // [3/4/2013]
    //=========================================================================
    string
    SocketDriverCommon::IPPortToAddressString(const char* ip, unsigned int port)
    {
        AZ_Assert(ip != nullptr, "Invalid address!");
        return string::format("%s|%d", ip, port);
    }

    //=========================================================================
    // CreateSocketDriver
    // [3/4/2013]
    //=========================================================================
    bool
    SocketDriverCommon::AddressStringToIPPort(const string& address, string& ip, unsigned int& port)
    {
        AZStd::size_t pos = address.find('|');
        AZ_Assert(pos != string::npos, "Invalid driver address!");
        if (pos == string::npos)
        {
            return false;
        }

        ip = string(address.begin(), address.begin() + pos);
        port = AZStd::stoi(string(address.begin() + pos + 1, address.end()));

        return true;
    }

    //=========================================================================
    // AddressFamilyType
    // [7/11/2013]
    //=========================================================================
    Driver::BSDSocketFamilyType
    SocketDriverCommon::AddressFamilyType(const string& ip)
    {
        // TODO: We can/should use inet_ntop() to detect the family type
        AZStd::size_t pos = ip.find(".");
        if (pos != string::npos)
        {
            return BSD_AF_INET;
        }
        pos = ip.find("::");
        if (pos != string::npos)
        {
            return BSD_AF_INET6;
        }
        return BSD_AF_UNSPEC;
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // Socket Driver
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //=========================================================================
    // CreateDriverAddress(const string&)
    // [1/12/2011]
    //=========================================================================
    AZStd::intrusive_ptr<DriverAddress>
    SocketDriver::CreateDriverAddress(const string& address)
    {
        string ip;
        unsigned int port;
        if (!AddressToIPPort(address, ip, port))
        {
            return NULL;
        }

        SocketDriverAddress drvAddr(this, ip, port);
        return &*m_addressMap.insert(drvAddr).first;
    }

    //=========================================================================
    // CreateDriverAddress(const sockaddr_in& addr)
    // [1/12/2011]
    //=========================================================================
    AZStd::intrusive_ptr<DriverAddress>
    SocketDriver::CreateDriverAddress(const sockaddr* sockAddr)
    {
        SocketDriverAddress drvAddr(this, sockAddr);
        return &*m_addressMap.insert(drvAddr).first;
    }

    //=========================================================================
    // DestroyDriverAddress
    // [1/12/2011]
    //=========================================================================
    void SocketDriver::DestroyDriverAddress(DriverAddress* address)
    {
        if (address)
        {
            AZ_Assert(address->GetDriver() == this, "The address %s doesn't belong to this driver!", address->ToString().c_str());
            SocketDriverAddress* socketAddress = static_cast<SocketDriverAddress*>(address);
            m_addressMap.erase(*socketAddress);
        }
    }

    namespace Utils
    {
        // \note function moved here to use addinfo when IPV6 is not in use, consider moving those definitions to a header file
        bool GetIpByHostName(int familyType, const char* hostName, string& ip)
        {
            static const size_t kMaxLen = 64; // max length of ipv6 ip is 45 chars, so all ips should be able to fit in this buf
            char ipBuf[kMaxLen];

            addrinfo hints;
            memset(&hints, 0, sizeof(struct addrinfo));
            addrinfo* addrInfo;
            hints.ai_family = (familyType == Driver::BSD_AF_INET6) ? AF_INET6 : AF_INET;
            hints.ai_flags = AI_CANONNAME;

            int result = getaddrinfo(hostName, nullptr, &hints, &addrInfo);
            if (result)
            {
                return false;
            }

            if (addrInfo->ai_family == AF_INET6)
            {
                ip = inet_ntop(hints.ai_family, &reinterpret_cast<sockaddr_in6*>(addrInfo->ai_addr)->sin6_addr, ipBuf, AZ_ARRAY_SIZE(ipBuf));
            }
            else
            {
                ip = inet_ntop(hints.ai_family, &reinterpret_cast<sockaddr_in*>(addrInfo->ai_addr)->sin_addr, ipBuf, AZ_ARRAY_SIZE(ipBuf));
            }

            freeaddrinfo(addrInfo);
            return true;
        }
    } //namespace Utils
} //namesapce GridMae

#endif // #ifndef AZ_UNITY_BUILD
