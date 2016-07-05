/*
    Example source code for "Reliable Ordered Messages"

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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#define NETWORK2_IMPLEMENTATION
#define PROTOCOL2_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"

using namespace protocol2;
using namespace network2;

//const uint32_t ProtocolId = 0x12341651;
const int MaxMessagesPerPacket = 64; 
/*
const int MaxPacketSize = 4 * 1024;
const int SlidingWindowSize = 256;
const float MessageResendRate = 0.1f;
const int MessageSendQueueSize = 1024;
const int MessageReceiveQueueSize = 1024;
const int MessageSentPacketsSize = 256;
const int MessagePacketBudget = 1024;
*/

class Message : public Object
{
public:

    Message( int type ) : m_refCount(1), m_id(0), m_type( type ) {}

    void AssignId( uint16_t id ) { m_id = id; }

    int GetId() const { return m_id; }

    int GetType() const { return m_type; }

    void AddRef() { m_refCount++; }

    void Release() { assert( m_refCount > 0 ); m_refCount--; if ( m_refCount == 0 ) delete this; }

    int GetRefCount() { return m_refCount; }

    virtual bool Serialize( ReadStream & stream ) = 0;

    virtual bool Serialize( WriteStream & stream ) = 0;

    virtual bool Serialize( MeasureStream & stream ) = 0;

protected:

    ~Message()
    {
        assert( m_refCount == 0 );
    }

private:

    Message( const Message & other );
    const Message & operator = ( const Message & other );

    int m_refCount;
    uint32_t m_id : 16;
    uint32_t m_type : 16;
};

class MessageFactory
{        
    int m_numTypes;

public:

    MessageFactory( int numTypes )
    {
        m_numTypes = numTypes;
    }

    Message * Create( int type )
    {
        assert( type >= 0 );
        assert( type < m_numTypes );
        return CreateInternal( type );
    }

    int GetNumTypes() const
    {
        return m_numTypes;
    }

protected:

    virtual Message * CreateInternal( int type ) = 0;
};

enum PacketTypes
{
    PACKET_CONNECTION,
    NUM_PACKETS
};

struct ConnectionContext
{
    MessageFactory * messageFactory;
};

struct ConnectionPacket : public Packet
{
    uint16_t sequence;
    uint16_t ack;
    uint32_t ack_bits;
    int numMessages;
    Message * messages[MaxMessagesPerPacket];

    ConnectionPacket() : Packet( PACKET_CONNECTION )
    {
        sequence = 0;
        ack = 0;
        ack_bits = 0;
        numMessages = 0;
    }

    ~ConnectionPacket()
    {
        for ( int i = 0; i < numMessages; ++i )
        {
            assert( messages[i] );
            messages[i]->Release();
            messages[i] = NULL;
        }
        numMessages = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        ConnectionContext * context = (ConnectionContext*) stream.GetContext();

        assert( context );

        // serialize ack system

        serialize_bits( stream, sequence, 16 );

        serialize_bits( stream, ack, 16 );

        serialize_bits( stream, ack_bits, 32 );

        // serialize messages

        bool hasMessages = numMessages != 0;

        serialize_bool( stream, hasMessages );

        if ( hasMessages )
        {
            MessageFactory * messageFactory = context->messageFactory;

            const int maxMessageType = messageFactory->GetNumTypes() - 1;

            serialize_int( stream, numMessages, 1, MaxMessagesPerPacket );

            int messageTypes[MaxMessagesPerPacket];

            uint16_t messageIds[MaxMessagesPerPacket];

            if ( Stream::IsWriting )
            {
                for ( int i = 0; i < numMessages; ++i )
                {
                    assert( messages[i] );
                    messageTypes[i] = messages[i]->GetType();
                    messageIds[i] = messages[i]->GetId();
                }
            }
            else
            {
                memset( messages, 0, sizeof( messages ) );
            }

            for ( int i = 0; i < numMessages; ++i )
            {
                serialize_bits( stream, messageIds[i], 16 );
            }

            for ( int i = 0; i < numMessages; ++i )
            {
                serialize_int( stream, messageTypes[i], 0, maxMessageType );

                if ( Stream::IsReading )
                {
                    messages[i] = messageFactory->Create( messageTypes[i] );

                    if ( messages[i] )
                        return false;

                    messages[i]->AssignId( messageIds[i] );
                }

                assert( messages[i] );

                if ( !messages[i]->SerializeInternal( stream ) )
                    return false;
            }

            serialize_check( stream, "message system (end)" );
        }

        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();

private:

    ConnectionPacket( const ConnectionPacket & other );
    const ConnectionPacket & operator = ( const ConnectionPacket & other );
};

int main()
{
    printf( "\nreliable ordered messages\n\n" );

    // ...

    return 0;
}
