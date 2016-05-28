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

#ifndef YOJIMBO_PACKET_WRITER_H
#define YOJIMBO_PACKET_WRITER_H

#include "protocol2.h"

namespace yojimbo
{
    class PacketProcessor
    {
    private:

        uint32_t m_protocolId;

        int m_maxPacketSize;
        int m_absoluteMaxPacketSize;
        
        uint8_t * m_packetBuffer;
        uint8_t * m_scratchBuffer;

        void * m_context;

        protocol2::PacketFactory * m_packetFactory;

    public:

        PacketProcessor( protocol2::PacketFactory & packetFactory, uint32_t protocolId, int maxPacketSize, void * context = NULL );

        ~PacketProcessor();

        const uint8_t * WritePacket( protocol2::Packet * packet, 
                                     uint64_t sequence,
                                     int & packetBytes,
                                     bool encrypt,
                                     const uint8_t * key );

        protocol2::Packet * ReadPacket( const uint8_t * packetData, 
                                        uint64_t & sequence,
                                        int packetBytes,
                                        const uint8_t * key,
                                        const uint8_t * encryptedPacketTypes,
                                        const uint8_t * unencryptedPacketTypes );

        int GetMaxPacketSize() const { return m_maxPacketSize; }
    };
}

#endif // #ifndef YOJIMBO_PACKET_WRITER
