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

#if YOJIMBO_SECURE
#include <sodium.h>
#endif // #if YOJIMBO_SECURE

#include <stdint.h>
#include <inttypes.h>

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

#if YOJIMBO_SECURE

        const int MaxPrefixBytes = 9;
        const int CryptoOverhead = MacBytes;

        m_absoluteMaxPacketSize = m_maxPacketSize + MaxPrefixBytes + CryptoOverhead;

#else // #if YOJIMBO_SECURE

        m_absoluteMaxPacketSize = m_maxPacketSize;

#endif // #if YOJIMBO_SECURE

        m_sendQueueSize = sendQueueSize;

        m_receiveQueueSize = receiveQueueSize;
        
        m_packetBuffer = (uint8_t*) m_allocator->Allocate( m_absoluteMaxPacketSize );
        
        m_packetFactory = &packetFactory;
        
        queue_reserve( m_sendQueue, sendQueueSize );
        queue_reserve( m_receiveQueue, receiveQueueSize );

#if YOJIMBO_SECURE

        const int numPacketTypes = m_packetFactory->GetNumPacketTypes();

		assert( numPacketTypes > 0 );

        m_packetTypeIsEncrypted = (uint8_t*) m_allocator->Allocate( numPacketTypes );
        m_packetTypeIsUnencrypted = (uint8_t*) m_allocator->Allocate( numPacketTypes );

        memset( m_packetTypeIsEncrypted, 0, m_packetFactory->GetNumPacketTypes() );
        memset( m_packetTypeIsUnencrypted, 1, m_packetFactory->GetNumPacketTypes() );

        m_numEncryptionMappings = 0;

#endif // #if YOJIMBO_SECURE

        memset( m_counters, 0, sizeof( m_counters ) );
    }

    SocketInterface::~SocketInterface()
    {
        assert( m_socket );
        assert( m_packetBuffer );
        assert( m_packetFactory );

        ClearSendQueue();
        ClearReceiveQueue();

        YOJIMBO_DELETE( *m_allocator, NetworkSocket, m_socket );

        m_allocator->Free( m_packetBuffer );

#if YOJIMBO_SECURE
        m_allocator->Free( m_packetTypeIsEncrypted );
        m_allocator->Free( m_packetTypeIsUnencrypted );
#endif // #if YOJIMBO_SECURE

        m_socket = NULL;
        m_packetBuffer = NULL;
        m_packetFactory = NULL;
#if YOJIMBO_SECURE
        m_packetTypeIsEncrypted = NULL;
        m_packetTypeIsUnencrypted = NULL;
#endif // #if YOJIMBO_SECURE

        m_allocator = NULL;
    }

    void SocketInterface::ClearSendQueue()
    {
        for ( int i = 0; i < (int) queue_size( m_sendQueue ); ++i )
        {
            PacketEntry & entry = m_sendQueue[i];
            assert( entry.packet );
            assert( entry.address.IsValid() );
            m_packetFactory->DestroyPacket( entry.packet );
        }

        queue_clear( m_sendQueue );
    }

    void SocketInterface::ClearReceiveQueue()
    {
        for ( int i = 0; i < (int) queue_size( m_receiveQueue ); ++i )
        {
            PacketEntry & entry = m_receiveQueue[i];
            assert( entry.packet );
            assert( entry.address.IsValid() );
            m_packetFactory->DestroyPacket( entry.packet );
        }

        queue_clear( m_receiveQueue );
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

#if YOJIMBO_SECURE
    static const int ENCRYPTED_PACKET_FLAG = (1<<7);
#endif // if YOJIMBO_SECURE

    static void PrintBytes( const uint8_t * data, int data_bytes )
    {
        for ( int i = 0; i < data_bytes; ++i )
        {
            printf( "%02x", (int) data[i] );
            if ( i != data_bytes - 1 )
                printf( "-" );
        }
        printf( " (%d bytes)", data_bytes );
    }

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

#if YOJIMBO_SECURE

            const bool encrypt = IsEncryptedPacketType( entry.packet->GetType() );

            uint8_t prefix[16];
            memset( prefix, 0, sizeof( prefix ) );
            int prefixBytes = 1;

            if ( encrypt )
            {
                yojimbo::CompressPacketSequence( entry.sequence, prefix[0], prefixBytes, prefix+1 );
                prefix[0] |= ENCRYPTED_PACKET_FLAG;
                prefixBytes++;
            }

#endif // #if YOJIMBO_SECURE

            protocol2::PacketInfo info;

            info.context = m_context;
            info.protocolId = m_protocolId;
            info.packetFactory = m_packetFactory;
#if YOJIMBO_SECURE
            info.prefixBytes = prefixBytes;
            info.rawFormat = encrypt;
#endif // #if YOJIMBO_SECURE

            int packetBytes = protocol2::WritePacket( info, entry.packet, m_packetBuffer, m_maxPacketSize );

            if ( packetBytes > 0 )
            {
                assert( packetBytes <= m_maxPacketSize );

#if YOJIMBO_SECURE
                if ( encrypt )
                {
                    EncryptionMapping * encryptionMapping = FindEncryptionMapping( entry.address );

                    if ( encryptionMapping )
                    {
                        int encryptedPacketSize;

                        printf( "packet %" PRIx64 ": ", entry.sequence );
                        PrintBytes( m_packetBuffer, packetBytes );
                        printf( "\n" );

                        if ( Encrypt( m_packetBuffer + prefixBytes, 
                                      packetBytes - prefixBytes, 
                                      m_packetBuffer + prefixBytes, 
                                      encryptedPacketSize, 
                                      (uint8_t*) &entry.sequence, encryptionMapping->sendKey ) )
                        {
                            packetBytes = prefixBytes + encryptedPacketSize;

                            assert( packetBytes <= m_absoluteMaxPacketSize );
     
                            memcpy( m_packetBuffer, prefix, prefixBytes );

                            printf( "send %" PRIx64 ": ", entry.sequence );
                            PrintBytes( m_packetBuffer, packetBytes );
                            printf( "\n" );

                            m_socket->SendPacket( entry.address, m_packetBuffer, packetBytes );

                            m_counters[SOCKET_INTERFACE_COUNTER_PACKETS_WRITTEN]++;
                            m_counters[SOCKET_INTERFACE_COUNTER_ENCRYPTED_PACKETS_WRITTEN]++;    
                        }
                        else
                        {
                            m_counters[SOCKET_INTERFACE_COUNTER_ENCRYPT_PACKET_FAILURES]++;
                        }
                    }
                    else
                    {
                        m_counters[SOCKET_INTERFACE_COUNTER_ENCRYPTION_MAPPING_FAILURES_SEND]++;
                    }
                }
                else
#endif // #if YOJIMBO_SECURE
                {
                    m_socket->SendPacket( entry.address, m_packetBuffer, packetBytes );

                    m_counters[SOCKET_INTERFACE_COUNTER_PACKETS_WRITTEN]++;
#if YOJIMBO_SECURE
                    m_counters[SOCKET_INTERFACE_COUNTER_UNENCRYPTED_PACKETS_WRITTEN]++;    
#endif // #if YOJIMBO_SECURE
                }
            }
            else
            {
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

            assert( packetBytes > 0 );

            if ( queue_size( m_receiveQueue ) == (size_t) m_receiveQueueSize )
            {
                m_counters[SOCKET_INTERFACE_COUNTER_RECEIVE_QUEUE_OVERFLOW]++;
                break;
            }

#if YOJIMBO_SECURE

            const uint8_t prefixByte = m_packetBuffer[0];

            const bool encrypted = ( prefixByte & ENCRYPTED_PACKET_FLAG ) != 0;

            int numPrefixBytes = 1;
				   
            if ( encrypted )
            {
                EncryptionMapping * encryptionMapping = FindEncryptionMapping( address );
                if ( !encryptionMapping )
                {
                    m_counters[SOCKET_INTERFACE_COUNTER_ENCRYPTION_MAPPING_FAILURES_RECEIVE]++;
                    continue;
                }

                const int sequenceBytes = GetPacketSequenceBytes( prefixByte );

                numPrefixBytes += sequenceBytes;

                uint64_t sequence = DecompressPacketSequence( prefixByte, m_packetBuffer + 1 );

                int decryptedPacketBytes;

                printf( "recv %" PRIx64 ": ", sequence );
                PrintBytes( m_packetBuffer, packetBytes );
                printf( "\n" );

                if ( !Decrypt( m_packetBuffer + numPrefixBytes, 
                               packetBytes - numPrefixBytes, 
                               m_packetBuffer + numPrefixBytes, 
                               decryptedPacketBytes, 
                               (uint8_t*)&sequence, encryptionMapping->receiveKey ) )
                {
                    m_counters[SOCKET_INTERFACE_COUNTER_DECRYPT_PACKET_FAILURES]++;
                    continue;
                }

                packetBytes = numPrefixBytes + decryptedPacketBytes;

                memset( m_packetBuffer + packetBytes, 0, MacBytes );

                printf( "decrypted %" PRIx64 ": ", sequence );
                PrintBytes( m_packetBuffer, packetBytes + MacBytes );
                printf( "\n" );
            }

#endif // #if YOJMIBO_SECURE

            protocol2::PacketInfo info;
            
            info.context = m_context;
            info.protocolId = m_protocolId;
            info.packetFactory = m_packetFactory;
#if YOJIMBO_SECURE
            info.prefixBytes = numPrefixBytes;
            info.rawFormat = encrypted;
            info.allowedPacketTypes = encrypted ? m_packetTypeIsEncrypted : m_packetTypeIsUnencrypted;
#endif // #if YOJIMBO_SECURE

            int readError;
            protocol2::Packet *packet = protocol2::ReadPacket( info, m_packetBuffer, packetBytes, NULL, &readError );
            if ( packet )
            {
                m_counters[SOCKET_INTERFACE_COUNTER_PACKETS_READ]++;

#if YOJIMBO_SECURE
                if ( encrypted )
                    m_counters[SOCKET_INTERFACE_COUNTER_ENCRYPTED_PACKETS_READ]++;    
                else
                    m_counters[SOCKET_INTERFACE_COUNTER_UNENCRYPTED_PACKETS_READ]++;    
#endif // #if YOJIMBO_SECURE
            }
            else
            {
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

    int SocketInterface::GetMaxPacketSize() const 
    {
        return m_maxPacketSize;
    }

    void SocketInterface::SetContext( void * context )
    {
        m_context = context;
    }

#if YOJIMBO_SECURE

    void SocketInterface::EnablePacketEncryption()
    {
        memset( m_packetTypeIsEncrypted, 1, m_packetFactory->GetNumPacketTypes() );
        memset( m_packetTypeIsUnencrypted, 0, m_packetFactory->GetNumPacketTypes() );
    }

    void SocketInterface::DisableEncryptionForPacketType( int type )
    {
        assert( type >= 0 );
        assert( type < m_packetFactory->GetNumPacketTypes() );
        m_packetTypeIsEncrypted[type] = 0;
        m_packetTypeIsUnencrypted[type] = 1;
    }

    bool SocketInterface::IsEncryptedPacketType( int type ) const
    {
        assert( type >= 0 );
        assert( type < m_packetFactory->GetNumPacketTypes() );
        return m_packetTypeIsEncrypted[type] != 0;
    }

    bool SocketInterface::AddEncryptionMapping( const network2::Address & address, const uint8_t * sendKey, const uint8_t * receiveKey )
    {
        EncryptionMapping *encryptionMapping = FindEncryptionMapping( address );
        if ( encryptionMapping )
        {
            encryptionMapping->address = address;
            memcpy( encryptionMapping->sendKey, sendKey, KeyBytes );
            memcpy( encryptionMapping->receiveKey, receiveKey, KeyBytes );
            return true;
        }

        assert( m_numEncryptionMappings >= 0 );
        assert( m_numEncryptionMappings <= MaxEncryptionMappings );

        if ( m_numEncryptionMappings == MaxEncryptionMappings )
            return false;

        encryptionMapping = &m_encryptionMappings[m_numEncryptionMappings++];
        encryptionMapping->address = address;
        memcpy( encryptionMapping->sendKey, sendKey, KeyBytes );
        memcpy( encryptionMapping->receiveKey, receiveKey, KeyBytes );

        return true;
    }

    bool SocketInterface::RemoveEncryptionMapping( const network2::Address & /*address*/ )
    {
        // todo: implement this and consider a different data structure. this is not great.
        assert( !"not implemented yet" );
        return false;
    }

#endif // #if YOJIMBO_SECURE

    uint64_t SocketInterface::GetCounter( int index ) const
    {
        assert( index >= 0 );
        assert( index < SOCKET_INTERFACE_COUNTER_NUM_COUNTERS );
        return m_counters[index];
    }
}