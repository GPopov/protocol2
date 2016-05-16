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

namespace yojimbo
{
    SocketInterface::SocketInterface( Allocator & allocator, 
                                      protocol2::PacketFactory & packetFactory, 
                                      uint16_t socketPort, 
                                      network2::SocketType socketType, 
                                      int maxPacketSize, 
                                      int sendQueueSize, 
                                      int receiveQueueSize )
        : m_sendQueue( allocator ),
          m_receiveQueue( allocator )
    {
        assert( maxPacketSize > 0 );
        assert( sendQueueSize > 0 );
        assert( receiveQueueSize > 0 );

        m_context = NULL;

        m_allocator = &allocator;
        
        m_socket = new network2::Socket( socketPort, socketType );          // todo: create using allocator
        
        m_maxPacketSize = maxPacketSize;
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

        if ( (int) queue::size( m_sendQueue ) >= m_sendQueueSize )
        {
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

    void SocketInterface::SendPackets( double /*time*/ )
    {
        assert( m_allocator );
        assert( m_packetFactory );

        while ( queue::size( m_sendQueue ) )
        {
            const PacketEntry & entry = m_sendQueue[0];

            assert( entry.packet );
            assert( entry.address.IsValid() );

            queue::consume( m_sendQueue, 1 );

            // ...

            /*
            uint8_t buffer[m_config.maxPacketSize];

            typedef protocol::WriteStream Stream;

            Stream stream( buffer, m_config.maxPacketSize );

            stream.SetContext( m_context );

            uint64_t protocolId = m_config.protocolId;
            serialize_uint64( stream, protocolId );

            const int maxPacketType = m_config.packetFactory->GetNumTypes() - 1;
            
            int packetType = packet->GetType();
            
            serialize_int( stream, packetType, 0, maxPacketType );
            
            stream.Align();

            packet->SerializeWrite( stream );

            stream.Check( 0x51246234 );

            stream.Flush();

            CORE_ASSERT( !stream.IsOverflow() );

            if ( stream.IsOverflow() )
            {
                m_counters[BSD_SOCKET_COUNTER_SERIALIZE_WRITE_OVERFLOW]++;
                m_config.packetFactory->Destroy( packet );
                continue;
            }

            const int bytes = stream.GetBytesProcessed();
            const uint8_t * data = stream.GetData();

            CORE_ASSERT( bytes <= m_config.maxPacketSize );
            if ( bytes > m_config.maxPacketSize )
            {
                m_counters[BSD_SOCKET_COUNTER_PACKET_TOO_LARGE_TO_SEND]++;
                m_config.packetFactory->Destroy( packet );
                continue;
            }

            SendPacketInternal( packet->GetAddress(), data, bytes );
            */

            m_packetFactory->DestroyPacket( entry.packet );
        }
    }

    void SocketInterface::ReceivePackets( double /*time*/ )
    {
        // actually receive the packets and queue them up in a receive queue
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
