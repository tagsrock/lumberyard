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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include "RemoteCompiler.h"
#include "../RenderCapabilities.h"

#include <unordered_map>

#include <AzCore/Socket/AzSocket.h>

#include <AzFramework/Network/SocketConnection.h>

namespace NRemoteCompiler
{
    uint32 CShaderSrv::m_LastWorkingServer = 0;
    // Note:  Cry's original source uses little endian as their internal communication endianness
    // so this new code will do the same.


    // RemoteProxyState: store the current state of things required to communicate to the remote server
    // via the Engine Connection, so that its outside this interface and protected from the details of it.
    class RemoteProxyState
    {
    public:
        unsigned int m_remoteRequestCRC;
        unsigned int m_remoteResponseCRC;
        unsigned int m_nextAssignedToken;
        bool m_unitTestMode;
        bool m_engineConnectionCallbackInstalled; // lazy-install it.

        typedef std::function<void(const void* payload, unsigned int payloadSize)> TResponseCallback;

        RemoteProxyState()
        {
            m_engineConnectionCallbackInstalled = false;
            m_unitTestMode = false;
            m_remoteRequestCRC = AZ_CRC("ShaderCompilerProxyRequest");
            m_remoteResponseCRC = AZ_CRC("ShaderCompilerProxyResponse");
            m_nextAssignedToken = 0;
        }

    #if defined(AZ_TESTS_ENABLED)
        void SetUnitTestMode(bool newMode)
        {
            m_unitTestMode = newMode;
        }
    #endif // AZ_TESTS_ENABLED

        bool SubmitRequestAndBlockForResponse(std::vector<uint8>& inout)
        {
            unsigned int chosenToken = m_nextAssignedToken++;
            AzFramework::SocketConnection* engineConnection = AzFramework::SocketConnection::GetInstance();

            if (!m_unitTestMode)
            {
                // if we're not in unit test mode, we NEED an engine connection
                if (!engineConnection)
                {
                    iLog->LogError("ERROR: CShaderSrv::Compile: no engine connection present, but r_AssetProcessorShaderCompiler is set in config!\n");
                    return false;
                }

                // install the callback the first time its needed:
                if (!m_engineConnectionCallbackInstalled)
                {
                    // (AddTypeCallback is assumed to be thread safe.)

                    m_engineConnectionCallbackInstalled = true;
                    engineConnection->AddMessageHandler(m_remoteResponseCRC, std::bind(&RemoteProxyState::OnReceiveRemoteResponse, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
                }
            }

            // the plan:  Create a wait event
            // then raise the event when we get a response from the server
            // then wait for the event to be raised.

            CryEvent waitEvent;

            {
                CryAutoLock<CryMutex> protector(m_mapProtector); // scoped lock on the map

                // now create an anonymous function that will copy the data and set the waitevent when the callback is triggered:
                m_responsesAwaitingCallback[chosenToken] = [&inout, &waitEvent](const void* payload, unsigned int payloadSize)
                    {
                        inout.resize(payloadSize);
                        memcpy(inout.data(), payload, payloadSize);
                        waitEvent.Set();
                    };
            }

#if defined(AZ_TESTS_ENABLED)
            if (m_unitTestMode)
            {
                // if we're unit testing, there WILL BE NO ENGINE CONNECTION
                // we must respond as if we're the engine connection
                // you can assume the payload has already been unit tested to conform.
                // instead, just write the data to the buffer as expected:

                std::vector<uint8> newData;

                if (memcmp(inout.data(), "empty", 5) == 0)
                {
                    // unit test to send empty
                }
                else if (memcmp(inout.data(), "incomplete", 10) == 0)
                {
                    // unit test to send incomplete data.
                    newData.push_back('x');
                }
                else if (memcmp(inout.data(), "corrupt", 7) == 0)
                {
                    // unit test to send corrupt data
                    std::string testString("CDCDCDCDCDCDCDCD");
                    newData.assign(testString.begin(), testString.end());
                }
                else if ((memcmp(inout.data(), "compile_failure", 15) == 0) || (memcmp(inout.data(), "success", 7) == 0))
                {
                    // simulate compile failure response
                    // [payload length 4 bytes (LITTLE ENDIAN) ] [status 1 byte] [payload]
                    // and payload consists of
                    //   [ uncompressed size (NETWORK BYTE ORDER) ] [payload (compressed)]
                    bool isFail = (memcmp(inout.data(), "compile_failure", 15) == 0);
                    std::string failreason("decompressed_plaintext");

                    size_t uncompressed_size = failreason.size();
                    size_t compressed_size = uncompressed_size;
                    std::vector<char> compressedData;
                    compressed_size = uncompressed_size * 2;
                    compressedData.resize(compressed_size);

                    gEnv->pSystem->CompressDataBlock(failreason.data(), failreason.size(), compressedData.data(), compressed_size);
                    compressedData.resize(compressed_size);

                    // first four bytes are payload size.
                    // firth byte is status
                    unsigned int payloadSize = 4 + compressed_size;
                    newData.resize(4 + 1 + payloadSize);
                    uint8 status_code = isFail ? 0x05 : 0x01; // 5 is fail, 1 is ok

                    unsigned int uncompressed_size_le = uncompressed_size;
                    SwapEndian(uncompressed_size_le);

                    memcpy(newData.data(), &payloadSize, 4);
                    memcpy(newData.data() + 4, &status_code, 0x01); // 0x05 = error compiling

                    memcpy(newData.data() + 4 + 1, &uncompressed_size_le, 4);
                    memcpy(newData.data() + 4 + 1 + 4, compressedData.data(), compressedData.size());
                }
                else
                {
                    newData.clear();
                }

                // place the messageID at the end:
                newData.resize(newData.size() + 4); // for the messageID
                unsigned int swappedToken = chosenToken;
                SwapEndian(swappedToken);
                memcpy(newData.data() + newData.size() - 4, &swappedToken, 4);

                OnReceiveRemoteResponse(m_remoteResponseCRC, newData.data(), newData.size());
            }
            else // note:  This else is inside the endif so that it only takes the second branch if UNIT TEST MODE is present.
#endif // AZ_TESTS_ENABLED
            {
                // append the messageID:
                inout.resize(inout.size() + 4); // for the messageID
                unsigned int swappedToken = chosenToken;
                SwapEndian(swappedToken);
                memcpy(inout.data() + inout.size() - 4, &swappedToken, 4);

                if (!engineConnection->SendMsg(m_remoteRequestCRC, inout.data(), inout.size()))
                {
                    iLog->LogError("ERROR: CShaderSrv::Compile: unable to send via engine connection, but r_AssetProcessorShaderCompiler is set in config!\n");
                    return false;
                }
            }

            if (!waitEvent.Wait(10000))
            {
                // failure to get response!
                iLog->LogError("ERROR: CShaderSrv::Compile: no response received!\n");
                CryAutoLock<CryMutex> protector(m_mapProtector);
                m_responsesAwaitingCallback.erase(chosenToken);
                return false;
            }

            // wait succeeded! We got a response!  Unblock and return!
            return true;
        }

    private:

        CryMutex m_mapProtector;
        std::unordered_map<unsigned int, TResponseCallback> m_responsesAwaitingCallback;

        void OnReceiveRemoteResponse(unsigned int messageID, const void* payload, unsigned int payloadSize)
        {
            // peel back to inner payload:
            if (payloadSize < 4)
            {
                // indicate error!
                iLog->LogError("Err: OnReceiveRemoteREsponse - truncated message from shader compiler proxy");
                return;
            }

            // last four bytes are expected to be the response ID
            const uint8* payload_start = reinterpret_cast<const uint8*>(payload);
            const uint8* end_payload = payload_start + payloadSize;
            const uint8* messageIDPtr = end_payload - 4;

            unsigned int responseId = *reinterpret_cast<const unsigned int*>(messageIDPtr);
            SwapEndian(responseId);

            CryAutoLock<CryMutex> protector(m_mapProtector);
            auto callbackToCall = m_responsesAwaitingCallback.find(responseId);
            if (callbackToCall == m_responsesAwaitingCallback.end())
            {
                iLog->LogError("WARN:  Unexpected response from shader compiler proxy.");
                return;
            }
            // give only the inner payload back to the callee!

            callbackToCall->second(payload_start, payloadSize - sizeof(unsigned int));
            m_responsesAwaitingCallback.erase(callbackToCall);
        }
    };
    CShaderSrv::CShaderSrv()
    {
#if defined(AZ_TESTS_ENABLED)
        m_unitTestMode = false;
#endif // AZ_TESTS_ENABLED
        Init();
    }

    void CShaderSrv::Init()
    {
        ScopedSwitchToGlobalHeap useGlobalHeap;

        static RemoteProxyState proxyState;
        m_remoteState = &proxyState;

        int result = AZ::AzSock::Startup();
        if (AZ::AzSock::SocketErrorOccured(result))
        {
            iLog->Log("ERROR: CShaderSrv::Init: Could not init root socket\n");
            return;
        }

        m_RequestLineRootFolder = "";

        ICVar* pGameFolder = gEnv->pConsole->GetCVar("sys_game_folder");
        ICVar* pCompilerFolderSuffix = CRenderer::CV_r_ShaderCompilerFolderSuffix;

        if (pGameFolder)
        {
            string folder = pGameFolder->GetString();
            folder.Trim();
            if (!folder.empty())
            {
                if (pCompilerFolderSuffix)
                {
                    string suffix = pCompilerFolderSuffix->GetString();
                    suffix.Trim();
                    folder.append(suffix);
                }
                m_RequestLineRootFolder = folder + string("/");
            }
        }

        if (m_RequestLineRootFolder.empty())
        {
            iLog->Log("ERROR: CShaderSrv::Init: Game folder has not been specified\n");
        }
    }

    CShaderSrv& CShaderSrv::Instance()
    {
        static CShaderSrv g_ShaderSrv;
        return g_ShaderSrv;
    }

    string CShaderSrv::CreateXMLNode(const string& rTag, const string& rValue)   const
    {
        string Tag = rTag;
        Tag += "=\"";
        Tag += rValue;
        Tag += "\" ";
        return Tag;
    }

    /*string CShaderSrv::CreateXMLDataNode(const string& rTag,const string& rValue) const
    {
        string Tag="<";
        Tag+=rTag;
        Tag+="><![CDATA[";
        Tag+=rValue;
        Tag+="]]>";
        return Tag;
    }*/

    string CShaderSrv::TransformToXML(const string& rIn)    const
    {
        string Out;
        for (size_t a = 0, Size = rIn.size(); a < Size; a++)
        {
            const char C = rIn.c_str()[a];
            if (C == '&')
            {
                Out += "&amp;";
            }
            else
            if (C == '<')
            {
                Out += "&lt;";
            }
            else
            if (C == '>')
            {
                Out += "&gt;";
            }
            else
            if (C == '\"')
            {
                Out += "&quot;";
            }
            else
            if (C == '\'')
            {
                Out += "&apos;";
            }
            else
            {
                Out += C;
            }
        }
        return Out;
    }

    bool CShaderSrv::CreateRequest(std::vector<uint8>& rVec,
        std::vector<std::pair<string, string> >& rNodes) const
    {
        string Request = "<?xml version=\"1.0\"?><Compile ";
        Request +=  CreateXMLNode("Version", TransformToXML("2.2"));
        for (size_t a = 0; a < rNodes.size(); a++)
        {
            Request +=  CreateXMLNode(rNodes[a].first, TransformToXML(rNodes[a].second));
        }

        Request +=  " />";
        rVec    =   std::vector<uint8>(Request.c_str(), &Request.c_str()[Request.size() + 1]);
        return true;
    }

    const char* CShaderSrv::GetPlatform() const
    {
        const char* szTarget = "unknown";
        if (CParserBin::m_nPlatform == SF_ORBIS)
        {
            szTarget = "ORBIS";
        }
        else
        if (CParserBin::m_nPlatform == SF_DURANGO)
        {
            szTarget = "DURANGO";
        }
        else
        if (CParserBin::m_nPlatform == SF_D3D11)
        {
            szTarget = "D3D11";
        }
        else
        if (CParserBin::m_nPlatform == SF_GL4)
        {
            szTarget = "GL4";
        }
        else
        if (CParserBin::m_nPlatform == SF_GLES3)
        {
#if defined(OPENGL_ES)
            uint32 glVersion = RenderCapabilities::GetDeviceGLVersion();
            AZ_Assert(glVersion >= DXGLES_VERSION_30, "Invalid OpenGL version %lu", static_cast<unsigned long>(glVersion));
            if (glVersion == DXGLES_VERSION_30)
            {
                szTarget = "GLES3_0";
            }
            else
            {
                szTarget = "GLES3_1";
            }
#endif // defined(OPENGL_ES)
        }
        // Confetti Nicholas Baldwin: adding metal shader language support
        else
        if (CParserBin::m_nPlatform == SF_METAL)
        {
            szTarget = "METAL";
        }

        return szTarget;
    }

    bool CShaderSrv::RequestLine(const SCacheCombination& cmb, const string& rLine) const
    {
        const string    List(string(GetPlatform()) + "/" + cmb.Name.c_str() + "ShaderList.txt");
        return RequestLine(List, rLine);
    }

    bool CShaderSrv::CommitPLCombinations(std::vector<SCacheCombination>&   rVec)
    {
        const uint32 STEPSIZE = 32;
        float T0    =   iTimer->GetAsyncCurTime();
        for (uint32 i = 0; i < rVec.size(); i += STEPSIZE)
        {
            string Line;
            string levelRequest;

            levelRequest.Format("<%d>%s", rVec[i].nCount, rVec[i].CacheName.c_str());
            //printf("CommitPL[%d] '%s'\n", i, levelRequest.c_str());
            Line = levelRequest;
            for (uint32 j = 1; j < STEPSIZE && i + j < rVec.size(); j++)
            {
                Line += string(";");
                levelRequest.Format("<%d>%s", rVec[i + j].nCount, rVec[i + j].CacheName.c_str());
                //printf("CommitPL[%d] '%s'\n", i+j, levelRequest.c_str());
                Line += levelRequest;
            }
            if (!RequestLine(rVec[i], Line))
            {
                return false;
            }
        }
        float T1    =   iTimer->GetAsyncCurTime();
        iLog->Log("%3.3f to commit %" PRISIZE_T " Combinations\n", T1 - T0, rVec.size());


        return true;
    }

    EServerError CShaderSrv::Compile(std::vector<uint8>& rVec,
        const char*                 pProfile,
        const char*                 pProgram,
        const char*                 pEntry,
        const char*                 pCompileFlags,
        const char*               pIdent) const
    {
        EServerError errCompile = ESOK;

        std::vector<uint8>  CompileData;
        std::vector<std::pair<string, string> > Nodes;
        Nodes.resize(9);
        Nodes[0]    =   std::pair<string, string>(string("JobType"), string("Compile"));
        Nodes[1]    =   std::pair<string, string>(string("Profile"), string(pProfile));
        Nodes[2]    =   std::pair<string, string>(string("Program"), string(pProgram));
        Nodes[3]    =   std::pair<string, string>(string("Entry"), string(pEntry));
        Nodes[4]    =   std::pair<string, string>(string("CompileFlags"), string(pCompileFlags));
        Nodes[5]    =   std::pair<string, string>(string("HashStop"), string("1"));
        Nodes[6]    =   std::pair<string, string>(string("ShaderRequest"), string(pIdent));
        Nodes[7]    =   std::pair<string, string>(string("Project"), string(m_RequestLineRootFolder.c_str()));
        Nodes[8]    =   std::pair<string, string>(string("Platform"), string(GetPlatform()));

        if (gRenDev->CV_r_ShaderEmailTags && gRenDev->CV_r_ShaderEmailTags->GetString() &&
            strlen(gRenDev->CV_r_ShaderEmailTags->GetString()) > 0)
        {
            Nodes.resize(Nodes.size() + 1);
            Nodes[Nodes.size() - 1]   =   std::pair<string, string>(string("Tags"), string(gRenDev->CV_r_ShaderEmailTags->GetString()));
        }

        if (gRenDev->CV_r_ShaderEmailCCs && gRenDev->CV_r_ShaderEmailCCs->GetString() &&
            strlen(gRenDev->CV_r_ShaderEmailCCs->GetString()) > 0)
        {
            Nodes.resize(Nodes.size() + 1);
            Nodes[Nodes.size() - 1]   =   std::pair<string, string>(string("EmailCCs"), string(gRenDev->CV_r_ShaderEmailCCs->GetString()));
        }

        if (gRenDev->CV_r_ShaderCompilerDontCache)
        {
            Nodes.resize(Nodes.size() + 1);
            Nodes[Nodes.size() - 1]   =   std::pair<string, string>(string("Caching"), string("0"));
        }
        //  Nodes[5]    =   std::pair<string,string>(string("ShaderRequest",string(pShaderRequestLine));
        int nRetries = 3;
        do
        {
            if (errCompile != ESOK)
            {
                Sleep(5000);
            }

            if (!CreateRequest(CompileData, Nodes))
            {
                iLog->LogError("ERROR: CShaderSrv::Compile: failed composing Request XML\n");
                return ESFailed;
            }

            errCompile = Send(CompileData);
        } while (errCompile == ESRecvFailed && nRetries-- > 0);

        rVec    =   CompileData;

        if (errCompile != ESOK)
        {
            bool logError = true;
            const char* why = "";
            switch (errCompile)
            {
            case ESNetworkError:
                why = "Network Error";
                break;
            case ESSendFailed:
                why = "Send Failed";
                break;
            case ESRecvFailed:
                why = "Receive Failed";
                break;
            case ESInvalidState:
                why = "Invalid Return State (compile issue ?!?)";
                break;
            case ESCompileError:
                logError = false;
                why = "";
                break;
            case ESFailed:
                why = "";
                break;
            }
            if (logError)
            {
                iLog->LogError("ERROR: CShaderSrv::Compile: failed to compile %s (%s)", pEntry, why);
            }
        }
        return errCompile;
    }

    bool CShaderSrv::RequestLine(const string& rList, const string& rString) const
    {
        if (!gRenDev->CV_r_shaderssubmitrequestline)
        {
            return true;
        }

        string list = m_RequestLineRootFolder + rList;

        std::vector<uint8>  CompileData;
        std::vector<std::pair<string, string> > Nodes;
        Nodes.resize(3);
        Nodes[0]    =   std::pair<string, string>(string("JobType"), string("RequestLine"));
        Nodes[1]    =   std::pair<string, string>(string("Platform"), list);
        Nodes[2]    =   std::pair<string, string>(string("ShaderRequest"), rString);
        if (!CreateRequest(CompileData, Nodes))
        {
            iLog->LogError("ERROR: CShaderSrv::RequestLine: failed composing Request XML\n");
            return false;
        }

        return (Send(CompileData) == ESOK);
    }

    bool CShaderSrv::Send(AZSOCKET Socket, const char* pBuffer, uint32 Size) const
    {
        //size_t w;
        size_t wTotal = 0;
        while (wTotal < Size)
        {
            int result = AZ::AzSock::Send(Socket, pBuffer + wTotal, Size - wTotal, 0);
            if (AZ::AzSock::SocketErrorOccured(result))
            {
                iLog->Log("ERROR:CShaderSrv::Send failed (%s)\n", AZ::AzSock::GetStringForError(result));
                return false;
            }
            wTotal += (size_t)result;
        }
        return true;
    }

    bool CShaderSrv::Send(AZSOCKET Socket, std::vector<uint8>& rCompileData)   const
    {
        const uint64 Size   =   static_cast<uint32>(rCompileData.size());
        return Send(Socket, (const char*)&Size, 8) &&
               Send(Socket, (const char*)&rCompileData[0], static_cast<uint32>(Size));
    }

#define MAX_TIME_TO_WAIT 100000

    EServerError CShaderSrv::Recv(AZSOCKET Socket, std::vector<uint8>& rCompileData)   const
    {
        const size_t Offset =   5;//version 2 has 4byte size and 1 byte state
        //  const uint32 Size   =   static_cast<uint32>(rCompileData.size());
        //  return  Send(Socket,(const char*)&Size,4) ||
        //      Send(Socket,(const char*)&rCompileData[0],Size);


        //  delete[] optionsBuffer;
        uint32 nMsgLength = 0;
        uint32 nTotalRecived = 0;
        const size_t    BLOCKSIZE   =   4 * 1024;
        const size_t    SIZELIMIT   =   1024 * 1024;
        rCompileData.resize(0);
        rCompileData.reserve(64 * 1024);
        int CurrentPos  =   0;
        while (rCompileData.size() < SIZELIMIT)
        {
            rCompileData.resize(CurrentPos + BLOCKSIZE);

            int Recived = SOCKET_ERROR;
            int waitingtime = 0;
            while (Recived < 0)
            {
                Recived = AZ::AzSock::Recv(Socket, reinterpret_cast<char*>(&rCompileData[CurrentPos]), BLOCKSIZE, 0);
                if (AZ::AzSock::SocketErrorOccured(Recived))
                {
                    AZ::AzSock::AzSockError error = AZ::AzSock::AzSockError(Recived);
                    if (error == AZ::AzSock::AzSockError::eASE_EWOULDBLOCK)
                    {
                        // are we out of time
                        if (waitingtime > MAX_TIME_TO_WAIT)
                        {
                            iLog->LogError("ERROR: CShaderSrv::Recv:  error in recv() from remote server. Out of time during waiting %d seconds on block, sys_net_errno=%s\n",
                                MAX_TIME_TO_WAIT, AZ::AzSock::GetStringForError(Recived));
                            return ESRecvFailed;
                        }

                        waitingtime += 5;

                        // sleep a bit and try again
                        Sleep(5);
                    }
                    else
                    {
                        // count on retry to fix this after a small sleep
                        iLog->LogError("ERROR: CShaderSrv::Recv:  error in recv() from remote server at offset %lu: sys_net_errno=%s\n",
                            (unsigned long)rCompileData.size(), AZ::AzSock::GetStringForError(Recived));
                        return ESRecvFailed;
                    }
                }
            }

            if (Recived >= 0)
            {
                nTotalRecived += Recived;
            }

            if (nTotalRecived >= 4)
            {
                nMsgLength = *(uint32*)&rCompileData[0] + Offset;
            }

            if (Recived == 0 || nTotalRecived == nMsgLength)
            {
                rCompileData.resize(nTotalRecived);
                break;
            }
            CurrentPos  +=  Recived;
        }

        return ProcessResponse(rCompileData);
    }

    // given a data vector, check to see if its an error or a success situation.
    // if its an error, replace the buffer with the uncompressed error string if possible.
    EServerError CShaderSrv::ProcessResponse(std::vector<uint8>& rCompileData) const
    {
        // so internally the message is like this
        // [payload length 4 bytes] [status 1 byte] [payload]
        // note that the length of the payload is given, not the total length of the message
        // which is actually payload length + [4 bytes + 1 byte status]

        const size_t OffsetToPayload = sizeof(unsigned int) + sizeof(uint8); // probably 5 bytes

        if (rCompileData.size() < OffsetToPayload)
        {
            iLog->LogError("ERROR: CShaderSrv::Recv:  compile data incomplete from server (only %i bytes received)\n", static_cast<int>(rCompileData.size()));
            rCompileData.clear();
            return ESRecvFailed;
        }

        uint32 payloadSize = *(uint32*)&rCompileData[0];
        uint8 state = rCompileData[4];

        if (payloadSize + OffsetToPayload != rCompileData.size())
        {
            iLog->LogError("ERROR: CShaderSrv::Recv:  compile data incomplete from server - expected %i bytes, got %i bytes\n", static_cast<int>(payloadSize + OffsetToPayload), static_cast<int>(rCompileData.size()));
            rCompileData.clear();
            return ESRecvFailed;
        }

        if (rCompileData.size() > OffsetToPayload) // ie, not a zero-byte payload
        {
            // move the payload to the beginning of the array, so that the first byte is the first byte of the payload
            memmove(&rCompileData[0], &rCompileData[OffsetToPayload], rCompileData.size() - OffsetToPayload);
            rCompileData.resize(rCompileData.size() - OffsetToPayload);
        }
        else
        {
            rCompileData.clear();
        }

        // decompress datablock if available.
        if (rCompileData.size() > sizeof(unsigned int)) // theres a datablock, which is compressed and there's enough data in there for a header of 4 bytes and payload
        {
            // [4 bytes - size of uncompressed message in network order][compressed payload message]
            // Decompress incoming payload
            std::vector<uint8> rCompressedData;
            rCompressedData.swap(rCompileData);

            uint32 nSrcUncompressedLen = *(uint32*)&rCompressedData[0];
            SwapEndian(nSrcUncompressedLen);

            size_t nUncompressedLen = (size_t)nSrcUncompressedLen;

            if (nUncompressedLen > 1000000)
            {
                // Shader too big, something is wrong.
                rCompileData.clear(); // don't propogate "something is wrong" data
                return ESFailed;
            }

            rCompileData.resize(nUncompressedLen);
            if (nUncompressedLen > 0)
            {
                if (!gEnv->pSystem->DecompressDataBlock(&rCompressedData[4], rCompressedData.size() - 4, &rCompileData[0], nUncompressedLen))
                {
                    rCompileData.clear(); // don't propogate corrupted data
                    return ESFailed;
                }
            }
        }

        if (state != 1) //1==ECSJS_DONE state on server, dont change!
        {
            // getting here means SOME sort of error occurred.
            // don't print compile errors here, they'll be handled later
            if (state == 5) //5==ECSJS_COMPILE_ERROR state on server, dont change!
            {
                return ESCompileError;
            }

            iLog->LogError("ERROR: CShaderSrv::Recv:  compile data contains invalid return status: state = %d \n", state);

            return ESInvalidState;
        }

        //  iLog->Log("Recv = %d",(unsigned long)rCompileData.size() );
        return ESOK;
    }

    void CShaderSrv::Tokenize(tdEntryVec& rRet, const string& Tokens, const string& Separator)    const
    {
        rRet.clear();
        string::size_type Pt;
        string::size_type Start = 0;
        string::size_type SSize =   Separator.size();

        while ((Pt = Tokens.find(Separator, Start)) != string::npos)
        {
            string  SubStr  =   Tokens.substr(Start, Pt - Start);
            rRet.push_back(SubStr);
            Start = Pt + SSize;
        }

        rRet.push_back(Tokens.substr(Start));
    }

    EServerError CShaderSrv::Send(std::vector<uint8>& rCompileData) const
    {
        if (rCompileData.size() > std::numeric_limits<int>::max())
        {
            iLog->LogError("ERROR: CShaderSrv::Compile: compile data too big to send.n");
            return ESFailed;
        }

        // this function expects to block until a response is received or failure occurs.
        AzFramework::SocketConnection* engineConnection = AzFramework::SocketConnection::GetInstance();
        bool useAssetProcessor = ((CRenderer::CV_r_AssetProcessorShaderCompiler != 0) && (engineConnection) && (engineConnection->IsConnected()));

#if defined(AZ_TESTS_ENABLED)
        if (m_unitTestMode)
        {
            useAssetProcessor = true; // always test asset processor-based code
        }
#endif // AZ_TESTS_ENABLED

        if (useAssetProcessor)
        {
            EServerError resultFromConnection = SendRequestViaEngineConnection(rCompileData);
            if (resultFromConnection != ESOK)
            {
                return resultFromConnection;
            }
        }
        else
        {
            EServerError resultFromSocket = SendRequestViaSocket(rCompileData);

            if (resultFromSocket != ESOK)
            {
                return resultFromSocket;
            }
        }

        if (rCompileData.size() < 4)
        {
            return ESFailed;
        }

        return ESOK;
    }

    EServerError CShaderSrv::SendRequestViaSocket(std::vector<uint8>& rCompileData) const
    {
        AZSOCKET Socket = AZ_SOCKET_INVALID;
        int Err = SOCKET_ERROR;

        // generate the list of servers to make the request to:
        tdEntryVec ServerVec;
        if (gRenDev->CV_r_ShaderCompilerServer)
        {
            Tokenize(ServerVec, gRenDev->CV_r_ShaderCompilerServer->GetString(), ";");
        }

        if (ServerVec.empty())
        {
            ServerVec.push_back("localhost");
        }

        //connect
        for (uint32 nRetries = m_LastWorkingServer; nRetries < m_LastWorkingServer + ServerVec.size() + 6; nRetries++)
        {
            string Server   =   ServerVec[nRetries % ServerVec.size()];
            Socket = AZ::AzSock::Socket();
            if (!AZ::AzSock::IsAzSocketValid(Socket))
            {
                iLog->LogError("ERROR: CShaderSrv::Compile: can't create client socket: error %s\n", AZ::AzSock::GetStringForError(Socket));
                return ESNetworkError;
            }

            AZ::AzSock::SetSocketOption(Socket, AZ::AzSock::AzSocketOption::REUSEADDR, true);

            AZ::AzSock::AzSocketAddress socketAddress;
            socketAddress.SetAddress(Server.c_str(), gRenDev->CV_r_ShaderCompilerPort);

            Err = AZ::AzSock::Connect(Socket, socketAddress);
            if (!AZ::AzSock::SocketErrorOccured(Err))
            {
                AZ::AzSock::AzSocketAddress socketAddress;
                int result = AZ::AzSock::GetSockName(Socket, socketAddress);
                if (AZ::AzSock::SocketErrorOccured(result))
                {
                    iLog->LogError("ERROR: CShaderSrv::Compile: invalid socket after trying to connect: error %i, sys_net_errno=%s\n", Err, AZ::AzSock::GetStringForError(result));
                }

                m_LastWorkingServer = nRetries % ServerVec.size();
                break;
            }
            else
            {
                //iLog->LogError("ERROR: CShaderSrv::Compile: could not connect to %s\n", Server.c_str());
                iLog->LogError("ERROR: CShaderSrv::Compile: could not connect to %s (sys_net_errno=%s, retrying %d)\n", Server.c_str(), AZ::AzSock::GetStringForError(Err), nRetries);

                // if buffer is full try sleeping a bit before retrying
                // (if you keep getting this issue then try using same shutdown mechanism as server is doing (see server code))
                // (for more info on windows side check : http://www.proxyplus.cz/faq/articles/EN/art10002.htm)
                if (Err == static_cast<AZ::s32>(AZ::AzSock::AzSockError::eASE_ENOBUFS))
                {
                    Sleep(5000);
                }

                //socketclose(s);
                //return (size_t)-1;
                AZ::AzSock::CloseSocket(Socket);
                Socket = AZ_SOCKET_INVALID;

                return ESNetworkError;
            }
        }

        if (Socket == AZ_SOCKET_INVALID)
        {
            rCompileData.resize(0);
            iLog->LogError("ERROR: CShaderSrv::Compile: invalid socket after trying to connect: sys_net_errno=%s\n", AZ::AzSock::GetStringForError(Err));
            return ESNetworkError;
        }

        if (!Send(Socket, rCompileData))
        {
            rCompileData.resize(0);
            AZ::AzSock::CloseSocket(Socket);
            return ESSendFailed;
        }

        EServerError    Error   =   Recv(Socket, rCompileData);
        /*
            // inform server that we are done receiving data
            const uint64 Result =   1;
            Send(Socket,(const char*)&Result,8);
        */

        // shutdown the client side of the socket because we are done listening
        Err = AZ::AzSock::Shutdown(Socket, SD_BOTH);
        if (Err == SOCKET_ERROR)
        {
#if defined(APPLE)
            // Mac OS X does not forgive calling shutdown on a closed socket, linux and
            // windows don't mind
            if (WSAGetLastError() != ENOTCONN)
            {
#endif
            iLog->LogError("ERROR: CShaderSrv::Compile: error shutting down socket: sys_net_errno=%s\n", AZ::AzSock::GetStringForError(Err));
            AZ::AzSock::CloseSocket(Socket);
            return ESNetworkError;
#if defined(APPLE)
        }
#endif
        }

        AZ::AzSock::CloseSocket(Socket);
        if (Error != ESOK)
        {
            return Error;
        }

        return ESOK;
    }

    bool CShaderSrv::EncapsulateRequestInEngineConnectionProtocol(std::vector<uint8>& rCompileData) const
    {
        if (rCompileData.empty())
        {
            iLog->LogError("ERROR: CShaderSrv::Compile: Engine Connection was unable to send the message - zero bytes size.");
            return false;
        }

        string serverList = gRenDev->CV_r_ShaderCompilerServer->GetString();
        unsigned int serverListLength = static_cast<unsigned int>(serverList.size());
        unsigned short serverPort = gRenDev->CV_r_ShaderCompilerPort;

        // we're packing at the end because sometimes, you don't need to copy the data in that case.
        std::size_t originalSize = rCompileData.size();

        //                                 a null     the string        a null       the  port         the length of the string
        rCompileData.resize(originalSize +   1    + serverListLength +    1    +  sizeof(serverPort) + sizeof(unsigned int));
        uint8* dataStart = rCompileData.data() + originalSize;
        *dataStart = 0; // null
        ++dataStart;
        memcpy(dataStart, serverList.c_str(), serverList.size()); // server list data
        dataStart += serverList.size();
        *dataStart = 0; // null
        ++dataStart;

        SwapEndian(serverPort);
        SwapEndian(serverListLength);

        memcpy(dataStart, &serverPort, sizeof(serverPort)); // server port
        dataStart += sizeof(serverPort);
        memcpy(dataStart, &serverListLength, sizeof(serverListLength)); // server list length
        dataStart += sizeof(serverListLength);

        // check for buffer overrun
        assert(reinterpret_cast<ptrdiff_t>(dataStart) - reinterpret_cast<ptrdiff_t>(rCompileData.data()) == rCompileData.size());
        return true;
    }

    EServerError CShaderSrv::SendRequestViaEngineConnection(std::vector<uint8>& rCompileData)   const
    {
        // use the asset processor instead of direct socket.
        // wrap it up in a protocol structure - very straight forward - the requestID followed by the data
        // the protocol already takes care of the data size, underneath, so no need to send that

        // what we need include the information about what server(s) to connect to.
        // we can append to the end of the compile data so as to avoid copying unless we need to

        if (!EncapsulateRequestInEngineConnectionProtocol(rCompileData))
        {
            return ESFailed;
        }

        if (!m_remoteState->SubmitRequestAndBlockForResponse(rCompileData))
        {
            rCompileData.clear();
            iLog->LogError("ERROR: CShaderSrv::Compile: Engine Connection was unable to send the message.");
            return ESNetworkError;
        }

        if (rCompileData.empty())
        {
            iLog->LogError("ERROR: CShaderSrv::Recv:  compile data empty from server (didn't receive anything)\n");
            return ESRecvFailed;
        }

        // Check for error embedded in the response!
        return ProcessResponse(rCompileData);
    }


#if defined(AZ_TESTS_ENABLED)

    void CShaderSrv::EnableUnitTestingMode(bool mode)
    {
        m_unitTestMode = mode;
        m_remoteState->SetUnitTestMode(mode);
    }

#endif // AZ_TESTS_ENABLED

} // end namespace

