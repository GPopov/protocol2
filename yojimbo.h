/*
    Yojimbo Client/Server Network Library.

    Copyright © 2016, The Network Protocol Company, Inc.
    
    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

        2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
           in the documentation and/or other materials provided with the distribution.

        3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived 
           from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef YOJIMBO_H
#define YOJIMBO_H

#ifdef YOJIMBO_IMPLEMENTATION
    #define NETWORK2_IMPLEMENTATION
    #define PROTOCOL2_IMPLEMENTATION
#endif // #ifdef YOJIMBO_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"

namespace yojimbo
{
    class NetworkInterface
    {
    public:

        virtual ~NetworkInterface() {}

        virtual protocol2::Packet * CreatePacket( int type ) = 0;

        virtual void DestroyPacket( protocol2::Packet * packet ) = 0;

        virtual void SendPacket( const network2::Address & address, protocol2::Packet * packet ) = 0;

        virtual protocol2::Packet * ReceivePacket( network2::Address & from ) = 0;

        virtual void SendPackets( double time ) = 0;

        virtual void ReceivePackets( double time ) = 0;

        virtual uint32_t GetMaxPacketSize() const = 0;

        virtual void SetContext( const void * context ) = 0;
    };

    class SocketInterface : public NetworkInterface
    {
        int m_maxPacketSize;
        uint8_t * m_receiveBuffer;
        network2::Socket * m_socket;
        protocol2::PacketFactory * m_packetFactory;

    public:

        SocketInterface( protocol2::PacketFactory & packetFactory, uint16_t socketPort, network2::SocketType socketType = network2::SOCKET_TYPE_IPV6, int maxPacketSize = 4 * 1024 );

        ~SocketInterface();

        bool IsError() const;

        int GetError() const;

        protocol2::Packet * CreatePacket( int type );

        void DestroyPacket( protocol2::Packet * packet );

        void SendPacket( const network2::Address & address, protocol2::Packet * packet );

        protocol2::Packet * ReceivePacket( network2::Address & from );

        void SendPackets( double time );

        void ReceivePackets( double time );

        uint32_t GetMaxPacketSize() const;

        void SetContext( const void * context );
    };
}

#endif // #ifndef YOJIMBO_H

// ======================================================================================================

#ifdef YOJIMBO_IMPLEMENTATION

namespace yojimbo
{
    SocketInterface::SocketInterface( protocol2::PacketFactory & packetFactory, uint16_t socketPort, network2::SocketType socketType, int maxPacketSize )
    {
        m_socket = new network2::Socket( socketPort, socketType );
        m_maxPacketSize = maxPacketSize;
        m_receiveBuffer = new uint8_t[maxPacketSize];
        m_packetFactory = &packetFactory;
    }

    SocketInterface::~SocketInterface()
    {
        assert( m_socket );
        assert( m_receiveBuffer );
        assert( m_packetFactory );
        delete m_socket;
        delete [] m_receiveBuffer;
        m_socket = NULL;
        m_receiveBuffer = NULL;
        m_packetFactory = NULL;
    }

    bool SocketInterface::IsError() const
    {
        assert( m_socket );
        return m_socket->IsError();
    }

    int SocketInterface::GetError() const
    {
        assert( m_socket );
        return m_socket->GetError();
    }

    protocol2::Packet * SocketInterface::CreatePacket( int type )
    {
        return m_packetFactory->CreatePacket( type );
    }

    void SocketInterface::DestroyPacket( protocol2::Packet * packet )
    {
        m_packetFactory->DestroyPacket( packet );
    }

    void SocketInterface::SendPacket( const network2::Address & address, protocol2::Packet * packet )
    {
        // ...
    }

    protocol2::Packet * SocketInterface::ReceivePacket( network2::Address & from )
    {
        return NULL;
    }

    void SocketInterface::SendPackets( double time )
    {
        // actually send the packets over the wire
    }

    void SocketInterface::ReceivePackets( double time )
    {
        // actually receive the packets and queue them up in a receive queue
    }

    uint32_t SocketInterface::GetMaxPacketSize() const 
    {
        return m_maxPacketSize;
    }

    void SocketInterface::SetContext( const void * context )
    {
        // ...
    }
}

#endif // #ifdef YOJIMBO_IMPLEMENTATION
