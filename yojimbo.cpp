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

#include "yojimbo.h"
#include "yojimbo_allocator.h"
#include "yojimbo_memory.h"    
#include "yojimbo_types.h"
#include "yojimbo_array.h"
#include "yojimbo_hash.h"
#include "yojimbo_queue.h"

#define NETWORK2_IMPLEMENTATION
#define PROTOCOL2_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"

namespace yojimbo
{
    SocketInterface::SocketInterface( Allocator & allocator, 
                                      protocol2::PacketFactory & packetFactory, 
                                      uint32_t protocolId,
                                      uint16_t socketPort, 
                                      network2::SocketType socketType, 
                                      int maxPacketSize, 
                                      int sendQueueSize, 
                                      int receiveQueueSize )
        : m_sendQueue( allocator ),
          m_receiveQueue( allocator )
    {
        assert( protocolId != 0 );
        assert( maxPacketSize > 0 );
        assert( sendQueueSize > 0 );
        assert( receiveQueueSize > 0 );

        m_context = NULL;

        m_allocator = &allocator;
        
        m_socket = new network2::Socket( socketPort, socketType );          // todo: create using allocator
        
        m_protocolId = protocolId;
        m_maxPacketSize = maxPacketSize;                            // todo: make sure multiple of dwords. round up.
        m_sendQueueSize = sendQueueSize;
        m_receiveQueueSize = receiveQueueSize;
        
        m_packetBuffer = new uint8_t[maxPacketSize];               // todo: use allocator
        
        m_packetFactory = &packetFactory;
        
        queue::reserve( m_sendQueue, sendQueueSize );
        queue::reserve( m_receiveQueue, receiveQueueSize );
    }

    SocketInterface::~SocketInterface()
    {
        assert( m_socket );
        assert( m_packetBuffer );
        assert( m_packetFactory );

        for ( size_t i = 0; i < queue::size( m_sendQueue ); ++i )
        {
            PacketEntry & entry = m_sendQueue[i];
            assert( entry.packet );
            assert( entry.address.IsValid() );
            m_packetFactory->DestroyPacket( entry.packet );
        }

        for ( size_t i = 0; i < queue::size( m_receiveQueue ); ++i )
        {
            PacketEntry & entry = m_receiveQueue[i];
            assert( entry.packet );
            assert( entry.address.IsValid() );
            m_packetFactory->DestroyPacket( entry.packet );
        }

        queue::clear( m_sendQueue );
        queue::clear( m_receiveQueue );

        delete m_socket;
        delete [] m_packetBuffer;              // todo: use allocator
        
        m_socket = NULL;
        m_packetBuffer = NULL;
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
        assert( m_packetFactory );
        return m_packetFactory->CreatePacket( type );
    }

    void SocketInterface::DestroyPacket( protocol2::Packet * packet )
    {
        assert( m_packetFactory );
        m_packetFactory->DestroyPacket( packet );
    }

    void SocketInterface::SendPacket( const network2::Address & address, protocol2::Packet * packet )
    {
        assert( m_allocator );
        assert( m_packetFactory );

        assert( packet );
        assert( address.IsValid() );

        if ( IsError() )
        {
            m_packetFactory->DestroyPacket( packet );
            return;
        }

        PacketEntry entry;
        entry.address = address;
        entry.packet = packet;

        if ( queue::size( m_sendQueue ) >= (size_t)m_sendQueueSize )
        {
            // todo: counter for packet send queue overflow
            m_packetFactory->DestroyPacket( packet );
            return;
        }

        queue::push_back( m_sendQueue, entry );
    }

    protocol2::Packet * SocketInterface::ReceivePacket( network2::Address & from )
    {
        assert( m_allocator );
        assert( m_packetFactory );

        if ( IsError() )
            return NULL;

        if ( queue::size( m_receiveQueue ) == 0 )
            return NULL;

        const PacketEntry & entry = m_receiveQueue[0];

        queue::consume( m_receiveQueue, 1 );

        assert( entry.packet );
        assert( entry.address.IsValid() );

        from = entry.address;

        return entry.packet;
    }

    void SocketInterface::WritePackets( double /*time*/ )
    {
        assert( m_allocator );
        assert( m_socket );
        assert( m_packetBuffer );
        assert( m_packetFactory );

        // todo: implement bandwidth limits here, eg. choke. can't send packets because no bandwidth available

        // todo: packet choke counter

        while ( queue::size( m_sendQueue ) )
        {
            const PacketEntry & entry = m_sendQueue[0];

            assert( entry.packet );
            assert( entry.address.IsValid() );

            queue::consume( m_sendQueue, 1 );

            bool error = false;

            // todo: need a way to pass in context to write packet

            const int bytesWritten = protocol2::WritePacket( entry.packet, m_packetFactory->GetNumTypes(), m_packetBuffer, m_maxPacketSize, m_protocolId );

            if ( bytesWritten > 0 )
            {
                // todo: get the packet type name here
                printf( "wrote packet type %d (%d bytes)\n", entry.packet->GetType(), bytesWritten );
                // todo: increase counter for packet written
            }
            else
            {
                printf( "write packet error\n" );
                m_packetFactory->DestroyPacket( entry.packet );
                // todo: increase counter for packet write failures
                error = true;
            }

            assert( bytesWritten > 0 );
            assert( bytesWritten <= m_maxPacketSize );

            m_socket->SendPacket( entry.address, m_packetBuffer, bytesWritten );

            m_packetFactory->DestroyPacket( entry.packet );
        }
    }

    void SocketInterface::ReadPackets( double /*time*/ )
    {
        assert( m_allocator );
        assert( m_socket );
        assert( m_packetBuffer );
        assert( m_packetFactory );

        // todo: extend the packet factory to provide custom allocation of packets (eg. through allocator), but don't put allocator in protocol2!

        // todo: put encryption inside protocol2 layer (PROTOCOL2_SECURE)

        // todo: must be able to discard unsupported packet types quickly, at this layer, before going further
        // (eg. on server, mark packet types expected to be received, and discard any other types...)

        // todo: encryption must be in the packet serialization layer (protocol2)

        // todo: but each client has a different private key... how to distinguish?

        // todo: seems like we need to pass a bunch of address/private key pairs to the packet factory for encrypted packets

        // wow. crazy.

        // urgh... client/server stuff spilling into protocol level.

        while ( true )
        {
            network2::Address address;
            int packetBytes = m_socket->ReceivePacket( address, m_packetBuffer, m_maxPacketSize );
            if ( !packetBytes )
                break;

            if ( queue::size( m_receiveQueue ) == (size_t) m_receiveQueueSize )
            {
                // todo: counter for receive queue overflow
                break;
            }

            // todo: need a way to pass in context to read packet

            int readError;
            protocol2::Packet *packet = protocol2::ReadPacket( *m_packetFactory, m_packetBuffer, packetBytes, m_protocolId, NULL, &readError );
            if ( packet )
            {
                // todo: get the packet type name here
                printf( "read packet type %d (%d bytes)\n", packet->GetType(), packetBytes );
                // todo: counter
            }
            else
            {
                printf( "read packet error: %s\n", protocol2::GetErrorString( readError ) );
                // todo: counter
                continue;
            }

            PacketEntry entry;
            entry.packet = packet;
            entry.address = address;
            queue::push_back( m_receiveQueue, entry );
        }
    }

    int SocketInterface::GetMaxPacketSize() const 
    {
        return m_maxPacketSize;
    }

    void SocketInterface::SetContext( const void * context )
    {
        m_context = context;
    }
}
