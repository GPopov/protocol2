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
#include "yojimbo_util.h"

#define NETWORK2_IMPLEMENTATION
#define PROTOCOL2_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"
#include <sodium.h>

namespace yojimbo
{
    typedef network2::Socket NetworkSocket;

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
        
        m_socket = YOJIMBO_NEW( *m_allocator, NetworkSocket, socketPort, socketType );
        
        m_protocolId = protocolId;

        m_maxPacketSize = maxPacketSize + ( ( maxPacketSize % 4 ) ? ( 4 - ( maxPacketSize % 4 ) ) : 0 );
        assert( m_maxPacketSize % 4 == 0 );
        assert( m_maxPacketSize >= maxPacketSize );

        m_sendQueueSize = sendQueueSize;

        m_receiveQueueSize = receiveQueueSize;
        
        m_packetBuffer = (uint8_t*) m_allocator->Allocate( m_maxPacketSize );
        
        m_packetFactory = &packetFactory;
        
        queue_reserve( m_sendQueue, sendQueueSize );
        queue_reserve( m_receiveQueue, receiveQueueSize );

        const int numPacketTypes = m_packetFactory->GetNumPacketTypes();
        m_packetTypeIsEncrypted = (uint8_t*) m_allocator->Allocate( numPacketTypes );
        memset( m_packetTypeIsEncrypted, 0, m_packetFactory->GetNumPacketTypes() );

        memset( m_counters, 0, sizeof( m_counters ) );

        memset( m_key, 1, sizeof( m_key ) );
    }

    SocketInterface::~SocketInterface()
    {
        assert( m_socket );
        assert( m_packetBuffer );
        assert( m_packetFactory );

        for ( size_t i = 0; i < queue_size( m_sendQueue ); ++i )
        {
            PacketEntry & entry = m_sendQueue[i];
            assert( entry.packet );
            assert( entry.address.IsValid() );
            m_packetFactory->DestroyPacket( entry.packet );
        }

        for ( size_t i = 0; i < queue_size( m_receiveQueue ); ++i )
        {
            PacketEntry & entry = m_receiveQueue[i];
            assert( entry.packet );
            assert( entry.address.IsValid() );
            m_packetFactory->DestroyPacket( entry.packet );
        }

        queue_clear( m_sendQueue );
        queue_clear( m_receiveQueue );

        YOJIMBO_DELETE( *m_allocator, NetworkSocket, m_socket );

        m_allocator->Free( m_packetBuffer );
        m_allocator->Free( m_packetTypeIsEncrypted );

        m_socket = NULL;
        m_packetBuffer = NULL;
        m_packetFactory = NULL;
        m_packetTypeIsEncrypted = NULL;

        m_allocator = NULL;
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

    void SocketInterface::SendPacket( const network2::Address & address, protocol2::Packet * packet, uint64_t sequence )
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
        entry.sequence = sequence;
        entry.address = address;
        entry.packet = packet;

        if ( queue_size( m_sendQueue ) >= (size_t)m_sendQueueSize )
        {
            m_counters[SOCKET_INTERFACE_COUNTER_SEND_QUEUE_OVERFLOW]++;
            m_packetFactory->DestroyPacket( packet );
            return;
        }

        queue_push_back( m_sendQueue, entry );

        m_counters[SOCKET_INTERFACE_COUNTER_PACKETS_SENT]++;
    }

    protocol2::Packet * SocketInterface::ReceivePacket( network2::Address & from, uint64_t * /*sequence*/ )
    {
        assert( m_allocator );
        assert( m_packetFactory );

        if ( IsError() )
            return NULL;

        if ( queue_size( m_receiveQueue ) == 0 )
            return NULL;

        const PacketEntry & entry = m_receiveQueue[0];

        queue_consume( m_receiveQueue, 1 );

        assert( entry.packet );
        assert( entry.address.IsValid() );

        from = entry.address;

        m_counters[SOCKET_INTERFACE_COUNTER_PACKETS_RECEIVED]++;

        return entry.packet;
    }

    static const int ENCRYPTED_PACKET_FLAG = (1<<7);

    void SocketInterface::WritePackets( double /*time*/ )
    {
        assert( m_allocator );
        assert( m_socket );
        assert( m_packetBuffer );
        assert( m_packetFactory );

        while ( queue_size( m_sendQueue ) )
        {
            const PacketEntry & entry = m_sendQueue[0];

            assert( entry.packet );
            assert( entry.address.IsValid() );

            queue_consume( m_sendQueue, 1 );

            const bool encrypt = IsEncryptedPacketType( entry.packet->GetType() );

            protocol2::PacketInfo info;

            info.context = m_context;
            info.protocolId = m_protocolId;
            info.packetFactory = m_packetFactory;
            info.prefixBytes = 1;
            info.rawFormat = encrypt;

            int bytesWritten = protocol2::WritePacket( info, entry.packet, m_packetBuffer, m_maxPacketSize );

            if ( bytesWritten > 0 )
            {
                assert( bytesWritten <= m_maxPacketSize );

                m_counters[SOCKET_INTERFACE_COUNTER_PACKETS_WRITTEN]++;

                if ( encrypt )
                {
                    // big big hack for testing
                    static int64_t sequence = 0;
                    sequence++;

                    printf( "encrypting packet: %lld\n", sequence/*entry.sequence*/ );

                    const int EncryptedPacketBufferSize = bytesWritten + MacBytes + 8;

                    uint8_t * encryptedPacket = (uint8_t*) alloca( EncryptedPacketBufferSize );

                    int encryptedPacketLength;
                    bool result = Encrypt( m_packetBuffer + 1, bytesWritten - 1, encryptedPacket + 1, encryptedPacketLength, (uint8_t*) &sequence/*entry.sequence*/, m_key );
                    if ( result )
                    {
                        printf( "encrypted bytes = %d\n", encryptedPacketLength );

                        encryptedPacketLength++;
                        encryptedPacket[0] = m_packetBuffer[0];                    

                        int sequenceBytes;
                        yojimbo::CompressPacketSequence( sequence/*entry.sequence*/, encryptedPacket[0], sequenceBytes, encryptedPacket + encryptedPacketLength );
                        encryptedPacketLength += sequenceBytes;

                        encryptedPacket[0] |= ENCRYPTED_PACKET_FLAG;

                        printf( "send sequence bytes = %d\n", sequenceBytes );

                        assert( encryptedPacketLength <= EncryptedPacketBufferSize );

                        m_socket->SendPacket( entry.address, encryptedPacket, encryptedPacketLength );
                    }
                    else
                    {
                        printf( "failed to encrypt packet\n" );
                    }
                }
                else
                {
                    m_socket->SendPacket( entry.address, m_packetBuffer, bytesWritten );
                }
            }
            else
            {
                printf( "failed to write packet (type %d)\n", entry.packet->GetType() );
                m_counters[SOCKET_INTERFACE_COUNTER_WRITE_PACKET_ERRORS]++;
            }

            m_packetFactory->DestroyPacket( entry.packet );
        }
    }

    void SocketInterface::ReadPackets( double /*time*/ )
    {
        assert( m_allocator );
        assert( m_socket );
        assert( m_packetBuffer );
        assert( m_packetFactory );

        while ( true )
        {
            network2::Address address;
            int packetBytes = m_socket->ReceivePacket( address, m_packetBuffer, m_maxPacketSize );
            if ( !packetBytes )
                break;

            if ( queue_size( m_receiveQueue ) == (size_t) m_receiveQueueSize )
            {
                m_counters[SOCKET_INTERFACE_COUNTER_RECEIVE_QUEUE_OVERFLOW]++;
                break;
            }

            const uint8_t prefixByte = m_packetBuffer[0];

            if ( prefixByte & ENCRYPTED_PACKET_FLAG )
            {
                int sequenceBytes = yojimbo::GetPacketSequenceBytes( prefixByte );

                printf( "recv sequence bytes = %d\n", sequenceBytes );

                uint64_t sequence = yojimbo::DecompressPacketSequence( prefixByte, m_packetBuffer + packetBytes - sequenceBytes );

                printf( "decrypting packet: %lld\n", sequence );

                packetBytes -= sequenceBytes;

                int decryptedPacketLength;
                uint8_t * decryptedPacket = (uint8_t*) alloca( packetBytes );                
                printf( "bytes to decrypt = %d\n", packetBytes - 1 );
                if ( Decrypt( m_packetBuffer + 1, packetBytes - 1, decryptedPacket, decryptedPacketLength, (uint8_t*) &sequence, m_key ) )
                {
                    protocol2::PacketInfo info;
                    
                    info.context = m_context;
                    info.protocolId = m_protocolId;
                    info.packetFactory = m_packetFactory;
                    info.prefixBytes = 1;
                    info.rawFormat = 1;

                    int readError;
                    protocol2::Packet *packet = protocol2::ReadPacket( info, decryptedPacket, decryptedPacketLength, NULL, &readError );
                    if ( packet )
                    {
                        m_counters[SOCKET_INTERFACE_COUNTER_PACKETS_READ]++;
                    }
                    else
                    {
                        printf( "read packet error: %s\n", protocol2::GetErrorString( readError ) );
                        m_counters[SOCKET_INTERFACE_COUNTER_READ_PACKET_ERRORS]++;
                        continue;
                    }

                    PacketEntry entry;
                    entry.sequence = sequence;
                    entry.packet = packet;
                    entry.address = address;
                    queue_push_back( m_receiveQueue, entry );
                }
                else
                {
                    printf( "failed to decrypt packet\n" );
                }
            }
            else
            {
                protocol2::PacketInfo info;
                
                info.context = m_context;
                info.protocolId = m_protocolId;
                info.packetFactory = m_packetFactory;
                info.prefixBytes = 1;
                info.rawFormat = 0;

                int readError;
                protocol2::Packet *packet = protocol2::ReadPacket( info, m_packetBuffer, packetBytes, NULL, &readError );
                if ( packet )
                {
                    m_counters[SOCKET_INTERFACE_COUNTER_PACKETS_READ]++;
                }
                else
                {
                    printf( "read packet error: %s\n", protocol2::GetErrorString( readError ) );
                    m_counters[SOCKET_INTERFACE_COUNTER_READ_PACKET_ERRORS]++;
                    continue;
                }

                PacketEntry entry;
                entry.sequence = 0;
                entry.packet = packet;
                entry.address = address;
                queue_push_back( m_receiveQueue, entry );
            }
        }
    }

    int SocketInterface::GetMaxPacketSize() const 
    {
        return m_maxPacketSize;
    }

    void SocketInterface::SetContext( void * context )
    {
        m_context = context;
    }

    void SocketInterface::EnablePacketEncryption()
    {
        memset( m_packetTypeIsEncrypted, 0xFF, m_packetFactory->GetNumPacketTypes() );
    }

    void SocketInterface::DisableEncryptionForPacketType( int type )
    {
        assert( type >= 0 );
        assert( type < m_packetFactory->GetNumPacketTypes() );
        m_packetTypeIsEncrypted[type] = 0;
    }

    bool SocketInterface::IsEncryptedPacketType( int type ) const
    {
        assert( type >= 0 );
        assert( type < m_packetFactory->GetNumPacketTypes() );
        // todo: for testing purposes only
        return true; //m_packetTypeIsEncrypted[type] != 0;
    }

    void SocketInterface::AddEncryptionMapping( const network2::Address & /*address*/, const uint8_t * /*sendKey*/, const uint8_t * /*receiveKey*/ )
    {
        // todo
    }

    void SocketInterface::RemoveEncryptionMapping( const network2::Address & /*address*/ )
    {
        // todo
    }
}
