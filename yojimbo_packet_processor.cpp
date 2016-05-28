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

#include "yojimbo_packet_processor.h"
#include "yojimbo_crypto.h"
#include "yojimbo_util.h"
#include "protocol2.h"
#include <stdio.h>

#if YOJIMBO_SECURE
#include <sodium.h>
#endif // #if YOJIMBO_SECURE

namespace yojimbo
{
    PacketProcessor::PacketProcessor( protocol2::PacketFactory & packetFactory, uint32_t protocolId, int maxPacketSize, void * context )
    {
        m_packetFactory = &packetFactory;
        m_protocolId = protocolId;

        m_maxPacketSize = maxPacketSize + ( ( maxPacketSize % 4 ) ? ( 4 - ( maxPacketSize % 4 ) ) : 0 );
        
        assert( m_maxPacketSize % 4 == 0 );
        assert( m_maxPacketSize >= maxPacketSize );

#if YOJIMBO_SECURE

        const int MaxPrefixBytes = 9;
        const int CryptoOverhead = MacBytes;
        m_absoluteMaxPacketSize = maxPacketSize + MaxPrefixBytes + CryptoOverhead;

#else // #if YOJIMBO_SECURE

        m_absoluteMaxPacketSize = maxPacketSize;

#endif // #if YOJIMBO_SECURE

        m_context = context;

        m_packetBuffer = new uint8_t[m_absoluteMaxPacketSize];
        m_scratchBuffer = new uint8_t[m_absoluteMaxPacketSize];
    }

    PacketProcessor::~PacketProcessor()
    {
        delete [] m_packetBuffer;
        delete [] m_scratchBuffer;

        m_packetBuffer = NULL;
        m_scratchBuffer = NULL;
    }

#if YOJIMBO_SECURE
    static const int ENCRYPTED_PACKET_FLAG = (1<<7);
#endif // if YOJIMBO_SECURE

    const uint8_t * PacketProcessor::WritePacket( protocol2::Packet * packet, 
                                                  uint64_t sequence,
                                                  int & packetBytes,
                                                  bool encrypt, 
                                                  const uint8_t * key )
    {
#if YOJIMBO_SECURE

        if ( encrypt )
        {
            if ( !key )
                return NULL;

            int prefixBytes;
            uint8_t prefix[16];
            CompressPacketSequence( sequence, prefix[0], prefixBytes, prefix+1 );
            prefix[0] |= ENCRYPTED_PACKET_FLAG;
            prefixBytes++;

            protocol2::PacketInfo info;

            info.context = m_context;
            info.protocolId = m_protocolId;
            info.packetFactory = m_packetFactory;
            info.rawFormat = 0;

            packetBytes = protocol2::WritePacket( info, packet, m_scratchBuffer, m_maxPacketSize );

            if ( packetBytes > 0 )
            {
                assert( packetBytes <= m_maxPacketSize );

                int encryptedPacketSize;

                if ( Encrypt( m_scratchBuffer,
                              packetBytes,
                              m_scratchBuffer,
                              encryptedPacketSize, 
                              (uint8_t*) &sequence, key ) )
                {
                    memcpy( m_packetBuffer, prefix, prefixBytes );
                    memcpy( m_packetBuffer + prefixBytes, m_scratchBuffer, encryptedPacketSize );

                    packetBytes = prefixBytes + encryptedPacketSize;

                    assert( packetBytes <= m_absoluteMaxPacketSize );

                    return m_packetBuffer;
                }
                else
                {
                    return NULL;
                }
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            protocol2::PacketInfo info;

            info.context = m_context;
            info.protocolId = m_protocolId;
            info.packetFactory = m_packetFactory;
            info.rawFormat = 0;
            info.prefixBytes = 1;

            packetBytes = protocol2::WritePacket( info, packet, m_packetBuffer, m_maxPacketSize );

            if ( packetBytes > 0 )
            {
                assert( packetBytes <= m_maxPacketSize );

                return m_packetBuffer;
            }
            else
            {
                return NULL;
            }
        }

#else // #if YOJIMBO_SECURE

        protocol2::PacketInfo info;

        info.context = m_context;
        info.protocolId = m_protocolId;
        info.packetFactory = m_packetFactory;

        packetBytes = protocol2::WritePacket( info, packet, m_packetBuffer, m_maxPacketSize );

        if ( packetBytes > 0 )
        {
            assert( packetBytes <= m_maxPacketSize );

            return m_packetBuffer;
        }

#endif // #if YOJIMBO_SECURE

        return m_packetBuffer;
    }

    protocol2::Packet * PacketProcessor::ReadPacket( const uint8_t * packetData, 
                                                     uint64_t & sequence,
                                                     int packetBytes,
                                                     const uint8_t * key,
                                                     const uint8_t * encryptedPacketTypes,
                                                     const uint8_t * unencryptedPacketTypes )
    {
#if YOJIMBO_SECURE

        const uint8_t prefixByte = packetData[0];

        const bool encrypted = ( prefixByte & ENCRYPTED_PACKET_FLAG ) != 0;

        if ( encrypted )
        {
            if ( !key )
                return NULL;

            const int sequenceBytes = GetPacketSequenceBytes( prefixByte );

            const int prefixBytes = 1 + sequenceBytes;

            if ( packetBytes <= prefixBytes + MacBytes )
                return NULL;

            sequence = DecompressPacketSequence( prefixByte, packetData + 1 );

            int decryptedPacketBytes;

            memcpy( m_scratchBuffer, packetData + prefixBytes, packetBytes - prefixBytes );

            if ( !Decrypt( m_scratchBuffer,
                           packetBytes - prefixBytes, 
                           m_scratchBuffer,
                           decryptedPacketBytes, 
                           (uint8_t*)&sequence, key ) )
            {
                return NULL;
            }

            protocol2::PacketInfo info;

            info.context = m_context;
            info.protocolId = m_protocolId;
            info.packetFactory = m_packetFactory;
            info.allowedPacketTypes = encryptedPacketTypes;
            info.rawFormat = 0;

            int readError;
            
            protocol2::Packet * packet = protocol2::ReadPacket( info, m_scratchBuffer, decryptedPacketBytes, NULL, &readError );
            
            return packet;
        }
        else
        {
            protocol2::PacketInfo info;
            
            info.context = m_context;
            info.protocolId = m_protocolId;
            info.packetFactory = m_packetFactory;
            info.allowedPacketTypes = unencryptedPacketTypes;
            info.prefixBytes = 1;

            int readError;

            protocol2::Packet * packet = protocol2::ReadPacket( info, packetData, packetBytes, NULL, &readError );

            sequence = 0;
            
            return packet;
        }

#else // #if YOJIMBO_SECURE

        protocol2::PacketInfo info;
        
        info.context = m_context;
        info.protocolId = m_protocolId;
        info.packetFactory = m_packetFactory;

        int readError;

        protocol2::Packet *packet = protocol2::ReadPacket( info, m_packetBuffer, packetBytes, NULL, &readError );

        return packet;

#endif // #if YOJIMBO_SECURE
    }
}
