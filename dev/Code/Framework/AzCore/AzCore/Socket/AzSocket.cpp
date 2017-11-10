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

#include <AzCore/Socket/AzSocket.h>

#if   defined(AZ_PLATFORM_ANDROID) 
#   define INVALID_SOCKET (-1)
#   define closesocket(_s)                          close(_s)
#   define GetInternalSocketError                   errno
typedef int SOCKET;
typedef AZ::s32 AZSOCKLEN;

#elif defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_LINUX)
#   define INVALID_SOCKET (-1)
#   define closesocket(_s)                          close(_s)
#   define GetInternalSocketError                   errno
typedef int SOCKET;
typedef AZ::u32 AZSOCKLEN;

#elif defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
#   define GetInternalSocketError                   WSAGetLastError()
typedef AZ::s32 AZSOCKLEN;
#pragma warning(push)
#pragma warning( disable : 4389 ) // warning C4389: '==' : signed/unsigned mismatch - caused by FD_SET in x86
#endif

namespace AZ
{
    namespace AzSock
    {
        AZ::s32 TranslateOSError(AZ::s32 oserror)
        {
            AZ::s32 error;

#define TRANSLATE(_from, _to) case (_from): error = static_cast<AZ::s32>(_to); break;

            switch (oserror)
            {
                TRANSLATE(0, AzSockError::eASE_NO_ERROR);

#if   defined(AZ_PLATFORM_ANDROID) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_LINUX)
                TRANSLATE(EACCES, AzSockError::eASE_EACCES);
                TRANSLATE(EADDRINUSE, AzSockError::eASE_EADDRINUSE);
                TRANSLATE(EADDRNOTAVAIL, AzSockError::eASE_EADDRNOTAVAIL);
                TRANSLATE(EAFNOSUPPORT, AzSockError::eASE_EAFNOSUPPORT);
                TRANSLATE(EALREADY, AzSockError::eASE_EALREADY);
                TRANSLATE(EBADF, AzSockError::eASE_EBADF);
                TRANSLATE(ECONNABORTED, AzSockError::eASE_ECONNABORTED);
                TRANSLATE(ECONNREFUSED, AzSockError::eASE_ECONNREFUSED);
                TRANSLATE(ECONNRESET, AzSockError::eASE_ECONNRESET);
                TRANSLATE(EFAULT, AzSockError::eASE_EFAULT);
                TRANSLATE(EHOSTDOWN, AzSockError::eASE_EHOSTDOWN);
                TRANSLATE(EINPROGRESS, AzSockError::eASE_EINPROGRESS);
                TRANSLATE(EINTR, AzSockError::eASE_EINTR);
                TRANSLATE(EINVAL, AzSockError::eASE_EINVAL);
                TRANSLATE(EISCONN, AzSockError::eASE_EISCONN);
                TRANSLATE(EMFILE, AzSockError::eASE_EMFILE);
                TRANSLATE(EMSGSIZE, AzSockError::eASE_EMSGSIZE);
                TRANSLATE(ENETUNREACH, AzSockError::eASE_ENETUNREACH);
                TRANSLATE(ENOBUFS, AzSockError::eASE_ENOBUFS);
                TRANSLATE(ENOPROTOOPT, AzSockError::eASE_ENOPROTOOPT);
                TRANSLATE(ENOTCONN, AzSockError::eASE_ENOTCONN);
                TRANSLATE(EOPNOTSUPP, AzSockError::eASE_EOPNOTSUPP);
                TRANSLATE(EPROTONOSUPPORT, AzSockError::eASE_EAFNOSUPPORT);
                TRANSLATE(ETIMEDOUT, AzSockError::eASE_ETIMEDOUT);
                TRANSLATE(ETOOMANYREFS, AzSockError::eASE_ETOOMANYREFS);
                TRANSLATE(EWOULDBLOCK, AzSockError::eASE_EWOULDBLOCK);

#elif defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
                TRANSLATE(WSAEACCES, AzSockError::eASE_EACCES);
                TRANSLATE(WSAEADDRINUSE, AzSockError::eASE_EADDRINUSE);
                TRANSLATE(WSAEADDRNOTAVAIL, AzSockError::eASE_EADDRNOTAVAIL);
                TRANSLATE(WSAEAFNOSUPPORT, AzSockError::eASE_EAFNOSUPPORT);
                TRANSLATE(WSAEALREADY, AzSockError::eASE_EALREADY);
                TRANSLATE(WSAEBADF, AzSockError::eASE_EBADF);
                TRANSLATE(WSAECONNABORTED, AzSockError::eASE_ECONNABORTED);
                TRANSLATE(WSAECONNREFUSED, AzSockError::eASE_ECONNREFUSED);
                TRANSLATE(WSAECONNRESET, AzSockError::eASE_ECONNRESET);
                TRANSLATE(WSAEFAULT, AzSockError::eASE_EFAULT);
                TRANSLATE(WSAEHOSTDOWN, AzSockError::eASE_EHOSTDOWN);
                TRANSLATE(WSAEINPROGRESS, AzSockError::eASE_EINPROGRESS);
                TRANSLATE(WSAEINTR, AzSockError::eASE_EINTR);
                TRANSLATE(WSAEINVAL, AzSockError::eASE_EINVAL);
                TRANSLATE(WSAEISCONN, AzSockError::eASE_EISCONN);
                TRANSLATE(WSAEMFILE, AzSockError::eASE_EMFILE);
                TRANSLATE(WSAEMSGSIZE, AzSockError::eASE_EMSGSIZE);
                TRANSLATE(WSAENETUNREACH, AzSockError::eASE_ENETUNREACH);
                TRANSLATE(WSAENOBUFS, AzSockError::eASE_ENOBUFS);
                TRANSLATE(WSAENOPROTOOPT, AzSockError::eASE_ENOPROTOOPT);
                TRANSLATE(WSAENOTCONN, AzSockError::eASE_ENOTCONN);
                TRANSLATE(WSAEOPNOTSUPP, AzSockError::eASE_EOPNOTSUPP);
                TRANSLATE(WSAEPROTONOSUPPORT, AzSockError::eASE_EAFNOSUPPORT);
                TRANSLATE(WSAETIMEDOUT, AzSockError::eASE_ETIMEDOUT);
                TRANSLATE(WSAETOOMANYREFS, AzSockError::eASE_ETOOMANYREFS);
                TRANSLATE(WSAEWOULDBLOCK, AzSockError::eASE_EWOULDBLOCK);

                // No equivalent in the posix api
                TRANSLATE(WSANOTINITIALISED, AzSockError::eASE_ENOTINITIALISED);
#endif

            default:
                AZ_TracePrintf("AzSock", "AzSocket could not translate OS error code %x, treating as miscellaneous.\n", oserror);
                error = static_cast<AZ::s32>(AzSockError::eASE_MISC_ERROR);
                break;
            }

#undef TRANSLATE

            return error;
        }

        AZ::s32 TranslateSocketOption(AzSocketOption opt)
        {
            AZ::s32 value;

#define TRANSLATE(_from, _to) case (_from): value = (_to); break;

            switch (opt)
            {
                TRANSLATE(AzSocketOption::REUSEADDR, SO_REUSEADDR);
                TRANSLATE(AzSocketOption::KEEPALIVE, SO_KEEPALIVE);
                TRANSLATE(AzSocketOption::LINGER, SO_LINGER);

            default:
                AZ_TracePrintf("AzSock", "AzSocket option %x not yet supported", opt);
                value = 0;
                break;
            }

#undef TRANSLATE

            return value;
        }

        AZSOCKET HandleInvalidSocket(SOCKET sock)
        {
            AZSOCKET azsock = static_cast<AZSOCKET>(sock);
            if (sock == INVALID_SOCKET)
            {
                azsock = TranslateOSError(GetInternalSocketError);
            }
            return azsock;
        }


        AZ::s32 HandleSocketError(AZ::s32 socketError)
        {

            if (socketError == SOCKET_ERROR)
            {
                socketError = TranslateOSError(GetInternalSocketError);
            }
            return socketError;
        }

        const char* GetStringForError(AZ::s32 errorNumber)
        {
            AzSockError errorCode = AzSockError(errorNumber);

#define CASE_RETSTRING(errorEnum) case errorEnum: { return #errorEnum; }

            switch (errorCode)
            {
                CASE_RETSTRING(AzSockError::eASE_NO_ERROR);
                CASE_RETSTRING(AzSockError::eASE_SOCKET_INVALID);
                CASE_RETSTRING(AzSockError::eASE_EACCES);
                CASE_RETSTRING(AzSockError::eASE_EADDRINUSE);
                CASE_RETSTRING(AzSockError::eASE_EADDRNOTAVAIL);
                CASE_RETSTRING(AzSockError::eASE_EAFNOSUPPORT);
                CASE_RETSTRING(AzSockError::eASE_EALREADY);
                CASE_RETSTRING(AzSockError::eASE_EBADF);
                CASE_RETSTRING(AzSockError::eASE_ECONNABORTED);
                CASE_RETSTRING(AzSockError::eASE_ECONNREFUSED);
                CASE_RETSTRING(AzSockError::eASE_ECONNRESET);
                CASE_RETSTRING(AzSockError::eASE_EFAULT);
                CASE_RETSTRING(AzSockError::eASE_EHOSTDOWN);
                CASE_RETSTRING(AzSockError::eASE_EINPROGRESS);
                CASE_RETSTRING(AzSockError::eASE_EINTR);
                CASE_RETSTRING(AzSockError::eASE_EINVAL);
                CASE_RETSTRING(AzSockError::eASE_EISCONN);
                CASE_RETSTRING(AzSockError::eASE_EMFILE);
                CASE_RETSTRING(AzSockError::eASE_EMSGSIZE);
                CASE_RETSTRING(AzSockError::eASE_ENETUNREACH);
                CASE_RETSTRING(AzSockError::eASE_ENOBUFS);
                CASE_RETSTRING(AzSockError::eASE_ENOPROTOOPT);
                CASE_RETSTRING(AzSockError::eASE_ENOTCONN);
                CASE_RETSTRING(AzSockError::eASE_ENOTINITIALISED);
                CASE_RETSTRING(AzSockError::eASE_EOPNOTSUPP);
                CASE_RETSTRING(AzSockError::eASE_EPIPE);
                CASE_RETSTRING(AzSockError::eASE_EPROTONOSUPPORT);
                CASE_RETSTRING(AzSockError::eASE_ETIMEDOUT);
                CASE_RETSTRING(AzSockError::eASE_ETOOMANYREFS);
                CASE_RETSTRING(AzSockError::eASE_EWOULDBLOCK);
                CASE_RETSTRING(AzSockError::eASE_EWOULDBLOCK_CONN);
                CASE_RETSTRING(AzSockError::eASE_MISC_ERROR);
            }

#undef CASE_RETSTRING

            return "(invalid)";
        }

        AZ::u32 HostToNetLong(AZ::u32 hstLong)
        {
            return htonl(hstLong);
        }

        AZ::u32 NetToHostLong(AZ::u32 netLong)
        {
            return ntohl(netLong);
        }

        AZ::u16 HostToNetShort(AZ::u16 hstShort)
        {
            return htons(hstShort);
        }

        AZ::u16 NetToHostShort(AZ::u16 netShort)
        {
            return ntohs(netShort);
        }

        AZ::s32 GetHostName(AZStd::string& hostname)
        {
            AZ::s32 result = 0;
            hostname.clear();

            char name[256];
            result = HandleSocketError(gethostname(name, AZ_ARRAY_SIZE(name)));
            if (result == static_cast<AZ::s32>(AzSockError::eASE_NO_ERROR))
            {
                hostname = name;
            }
            return result;
        }

        AZSOCKET Socket()
        {
            return Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }

        AZSOCKET Socket(AZ::s32 af, AZ::s32 type, AZ::s32 protocol)
        {
            return HandleInvalidSocket(socket(af, type, protocol));
        }

        AZ::s32 SetSockOpt(AZSOCKET sock, AZ::s32 level, AZ::s32 optname, const char* optval, AZ::s32 optlen)
        {
            AZSOCKLEN length(optlen);
            return HandleSocketError(setsockopt(sock, level, optname, optval, length));
        }

        AZ::s32 SetSocketOption(AZSOCKET sock, AzSocketOption opt, bool enable)
        {
            AZ::u32 val = enable ? 1 : 0;
            return SetSockOpt(sock, SOL_SOCKET, TranslateSocketOption(opt), reinterpret_cast<const char*>(&val), sizeof(val));
        }

        AZ::s32 EnableTCPNoDelay(AZSOCKET sock, bool enable)
        {
            AZ::u32 val = enable ? 1 : 0;
            return SetSockOpt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&val), sizeof(val));
        }

        AZ::s32 SetSocketBlockingMode(AZSOCKET sock, bool blocking)
        {
#if   defined(AZ_PLATFORM_ANDROID) || defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_LINUX)
            AZ::s32 flags = ::fcntl(sock, F_GETFL);
            flags &= ~O_NONBLOCK;
            flags |= (blocking ? 0 : O_NONBLOCK);
            return ::fcntl(sock, F_SETFL, flags);
#elif defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
            u_long val = blocking ? 0 : 1;
            return HandleSocketError(ioctlsocket(sock, FIONBIO, &val));
#else
#error Platform not supported!
            return 0;
#endif
        }

        AZ::s32 CloseSocket(AZSOCKET sock)
        {
            return HandleSocketError(closesocket(sock));
        }

        AZ::s32 Shutdown(AZSOCKET sock, AZ::s32 how)
        {
            return HandleSocketError(shutdown(sock, how));
        }

        AZ::s32 GetSockName(AZSOCKET sock, AzSocketAddress& addr)
        {
            AZSOCKADDR sAddr;
            AZSOCKLEN sAddrLen = sizeof(AZSOCKADDR);
            memset(&sAddr, 0, sAddrLen);
            AZ::s32 result = HandleSocketError(getsockname(sock, &sAddr, &sAddrLen));
            addr = sAddr;
            return result;
        }

        AZ::s32 Connect(AZSOCKET sock, const AzSocketAddress& addr)
        {
            AZ::s32 err = HandleSocketError(connect(sock, addr.GetTargetAddress(), sizeof(AZSOCKADDR_IN)));
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
            if (err == static_cast<AZ::s32>(AzSockError::eASE_EWOULDBLOCK))
#else
            if (err == static_cast<AZ::s32>(AzSockError::eASE_EINPROGRESS))
#endif
            {
                err = static_cast<AZ::s32>(AzSockError::eASE_EWOULDBLOCK_CONN);
            }
            return err;
        }

        AZ::s32 Listen(AZSOCKET sock, AZ::s32 backlog)
        {
            return HandleSocketError(listen(sock, backlog));
        }

        AZSOCKET Accept(AZSOCKET sock, AzSocketAddress& addr)
        {
            AZSOCKADDR sAddr;
            AZSOCKLEN sAddrLen = sizeof(AZSOCKADDR);
            memset(&sAddr, 0, sAddrLen);
            AZSOCKET outSock = HandleInvalidSocket(accept(sock, &sAddr, &sAddrLen));
            addr = sAddr;
            return outSock;
        }

        AZ::s32 Send(AZSOCKET sock, const char* buf, AZ::s32 len, AZ::s32 flags)
        {
#if   defined(AZ_PLATFORM_ANDROID) || defined(AZ_PLATFORM_LINUX)
            AZ::s32 msgNoSignal = MSG_NOSIGNAL;
#elif defined(AZ_PLATFORM_APPLE) || defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
            AZ::s32 msgNoSignal = 0;
#endif

            return HandleSocketError(send(sock, buf, len, flags | msgNoSignal));
        }

        AZ::s32 Recv(AZSOCKET sock, char* buf, AZ::s32 len, AZ::s32 flags)
        {
            return HandleSocketError(recv(sock, buf, len, flags));
        }

        AZ::s32 Bind(AZSOCKET sock, const AzSocketAddress& addr)
        {
            return HandleSocketError(bind(sock, addr.GetTargetAddress(), sizeof(AZSOCKADDR_IN)));
        }

        AZ::s32 Select(AZSOCKET sock, AZFD_SET* readfdsock, AZFD_SET* writefdsock, AZFD_SET* exceptfdsock, AZTIMEVAL* timeout)
        {
            return HandleSocketError(::select(sock + 1, readfdsock, writefdsock, exceptfdsock, timeout));
        }

        AZ::s32 IsRecvPending(AZSOCKET sock, AZTIMEVAL* timeout)
        {
            AZFD_SET readSet;
            FD_ZERO(&readSet);
            FD_SET(sock, &readSet);

            AZ::s32 ret = Select(sock, &readSet, nullptr, nullptr, timeout);
            if (ret >= 0)
            {
                ret = FD_ISSET(sock, &readSet);
                if (ret != 0)
                {
                    ret = 1;
                }
            }

            return ret;
        }

        AZ::s32 WaitForWritableSocket(AZSOCKET sock, AZTIMEVAL* timeout)
        {
            AZFD_SET writeSet;
            FD_ZERO(&writeSet);
            FD_SET(sock, &writeSet);

            AZ::s32 ret = Select(sock, nullptr, &writeSet, nullptr, timeout);
            if (ret >= 0)
            {
                ret = FD_ISSET(sock, &writeSet);
                if (ret != 0)
                {
                    ret = 1;
                }
            }

            return ret;
        }

        AZ::s32 Startup()
        {
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
            WSAData wsaData;
            return TranslateOSError(WSAStartup(MAKEWORD(2, 2), &wsaData));
#else
            return static_cast<AZ::s32>(AzSockError::eASE_NO_ERROR);
#endif
        }

        AZ::s32 Cleanup()
        {
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
            return TranslateOSError(WSACleanup());
#else
            return static_cast<AZ::s32>(AzSockError::eASE_NO_ERROR);
#endif
        }

        AzSocketAddress::AzSocketAddress()
        {
            Reset();
        }

        AzSocketAddress& AzSocketAddress::operator=(const AZSOCKADDR& addr)
        {
            m_sockAddr = *reinterpret_cast<const AZSOCKADDR_IN*>(&addr);
            return *this;
        }

        bool AzSocketAddress::operator==(const AzSocketAddress& rhs) const
        {
            return m_sockAddr.sin_family == rhs.m_sockAddr.sin_family
                && m_sockAddr.sin_addr.s_addr == rhs.m_sockAddr.sin_addr.s_addr
                && m_sockAddr.sin_port == rhs.m_sockAddr.sin_port;
        }

        const AZSOCKADDR* AzSocketAddress::GetTargetAddress() const
        {
            return reinterpret_cast<const AZSOCKADDR*>(&m_sockAddr);
        }

        AZStd::string AzSocketAddress::GetIP() const
        {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, const_cast<in_addr*>(&m_sockAddr.sin_addr), ip, AZ_ARRAY_SIZE(ip));
            return AZStd::string(ip);
        }

        AZStd::string AzSocketAddress::GetAddress() const
        {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, const_cast<in_addr*>(&m_sockAddr.sin_addr), ip, AZ_ARRAY_SIZE(ip));
            return AZStd::string::format("%s:%d", ip, ntohs(m_sockAddr.sin_port));
        }

        AZ::u16 AzSocketAddress::GetAddrPort() const
        {
            return ntohs(m_sockAddr.sin_port);
        }

        void AzSocketAddress::SetAddrPort(AZ::u16 port)
        {
            m_sockAddr.sin_port = htons(port);
        }

        bool AzSocketAddress::SetAddress(const AZStd::string& ip, AZ::u16 port)
        {
            AZ_Assert(!ip.empty(), "Invalid address string!");
            bool foundAddr = false;

            // resolve address
            {
                addrinfo hints;
                memset(&hints, 0, sizeof(addrinfo));
                addrinfo* addrInfo;
                hints.ai_family = AF_INET;
                hints.ai_flags = AI_CANONNAME;
                char strPort[8];
                azsnprintf(strPort, AZ_ARRAY_SIZE(strPort), "%d", port);

                const char* address = ip.c_str();

                if (address && strlen(address) == 0) // getaddrinfo doesn't accept empty string
                {
                    address = nullptr;
                }

                AZ::s32 err = HandleSocketError(getaddrinfo(address, strPort, &hints, &addrInfo));
                if (err == 0) // eASE_NO_ERROR
                {
                    if (addrInfo->ai_family == AF_INET)
                    {
                        m_sockAddr = *reinterpret_cast<const AZSOCKADDR_IN*>(addrInfo->ai_addr);
                        foundAddr = true;
                    }

                    freeaddrinfo(addrInfo);
                }
                else
                {
                    AZ_Assert(false, "AzSocketAddress could not resolve address %s with port %d. (reason - %s)", ip.c_str(), port, GetStringForError(err));
                }
            }
            return foundAddr;
        }

        bool AzSocketAddress::SetAddress(AZ::u32 ip, AZ::u16 port)
        {
            Reset();
            m_sockAddr.sin_addr.s_addr = htonl(ip);
            m_sockAddr.sin_port = htons(port);
            return true;
        }

        void AzSocketAddress::Reset()
        {
            memset(&m_sockAddr, 0, sizeof(m_sockAddr));
            m_sockAddr.sin_family = AF_INET;
            m_sockAddr.sin_addr.s_addr = INADDR_ANY;
        }
    }; // namespace AzSock
}; //namespace AZ

/**
* Windows platform-specific net modules
*/
#if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
#   pragma comment(lib,"WS2_32.lib")
#   pragma warning(pop)
#endif  // AZ_PLATFORM_WINDOWS
