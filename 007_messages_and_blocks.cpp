/*
    Example source code for "Messages and Blocks"

    Copyright © 2016, The Network Protocol Company, Inc.
    
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

#define NETWORK2_IMPLEMENTATION
#define PROTOCOL2_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

using namespace protocol2;
using namespace network2;

const int MaxMessagesPerPacket = 64; 
const int SlidingWindowSize = 256;
const int MessageSendQueueSize = 1024;
const int MessageReceiveQueueSize = 1024;
const int MessagePacketBudget = 1024;
const float MessageResendRate = 0.1f;
const int MaxBlockSize = 256 * 1024;
const int BlockFragmentSize = 1024;
const int MaxFragmentsPerBlock = MaxBlockSize / BlockFragmentSize;

class Message : public Object
{
public:

    Message( int type, bool block = false ) : m_refCount(1), m_id(0), m_type( type ), m_block( block ) {}

    void AssignId( uint16_t id ) { m_id = id; }

    int GetId() const { return m_id; }

    int GetType() const { return m_type; }

    void AddRef() { m_refCount++; }

    void Release() { assert( m_refCount > 0 ); m_refCount--; if ( m_refCount == 0 ) delete this; }

    int GetRefCount() const { return m_refCount; }

    bool IsBlockMessage() const { return m_block != 0; }

    virtual bool SerializeInternal( ReadStream & stream ) = 0;

    virtual bool SerializeInternal( WriteStream & stream ) = 0;

    virtual bool SerializeInternal( MeasureStream & stream ) = 0;

protected:

    virtual ~Message()
    {
        assert( m_refCount == 0 );
    }

private:

    Message( const Message & other );

    const Message & operator = ( const Message & other );

    int m_refCount;
    uint32_t m_id : 16;
    uint32_t m_type : 15;
    uint32_t m_block : 1;
};

class BlockMessage : public Message
{
public:

    BlockMessage( int type ) : Message( type, true ), m_blockSize( 0 ), m_blockData( NULL ) {}

    ~BlockMessage()
    {
        Disconnect();
    }

    void Connect( uint8_t * blockData, int blockSize )
    {
        Disconnect();

        assert( blockData );
        assert( blockSize > 0 );

        m_blockData = blockData;
        m_blockSize = blockSize;
    }

    void Disconnect()
    {
        if ( m_blockData )
        {
            delete [] m_blockData;
            m_blockData = NULL;
            m_blockSize = 0;
        }
    }

    bool SerializeInternal( ReadStream & /*stream*/ ) { assert( false ); return false; }

    bool SerializeInternal( WriteStream & /*stream*/ ) { assert( false ); return false; }

    bool SerializeInternal( MeasureStream & /*stream*/ ) { assert( false ); return false; }

    uint8_t * GetBlockData()
    {
        return m_blockData;
    }

    int GetBlockSize() const
    {
        return m_blockSize;
    }

private:

    int m_blockSize;
    uint8_t * m_blockData;
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
    CONNECTION_PACKET,
    NUM_PACKET_TYPES
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
    uint16_t oldest_message_id;
    int numMessages;
    Message * messages[MaxMessagesPerPacket];

    ConnectionPacket() : Packet( CONNECTION_PACKET )
    {
        sequence = 0;
        ack = 0;
        ack_bits = 0;
        oldest_message_id = 0xFFFF;
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

        serialize_bits( stream, oldest_message_id, 16 );

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
        }

        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();

private:

    ConnectionPacket( const ConnectionPacket & other );

    const ConnectionPacket & operator = ( const ConnectionPacket & other );
};

enum ConnectionError
{
    CONNECTION_ERROR_NONE = 0,
    CONNECTION_ERROR_MESSAGE_DESYNC,
    CONNECTION_ERROR_MESSAGE_SEND_QUEUE_FULL,
    CONNECTION_ERROR_MESSAGE_SERIALIZE_MEASURE_FAILED,
};

class Connection
{
public:

    Connection( PacketFactory & packetFactory, MessageFactory & messageFactory );

    ~Connection();

    void Reset();

    bool CanSendMessage() const;

    void SendMessage( Message * message );

    Message * ReceiveMessage();

    ConnectionPacket * WritePacket();

    bool ReadPacket( ConnectionPacket * packet );

    void AdvanceTime( double time );

    ConnectionError GetError() const;

protected:

    struct SentPacketData { uint8_t acked; };

    struct ReceivedPacketData {};

    struct MessageSendQueueEntry
    {
        Message * message;
        double timeLastSent;
        uint32_t measuredBits : 31;
        uint32_t block : 1;
    };

    struct MessageSentPacketEntry
    {
        double timeSent;
        uint16_t * messageIds;
        uint32_t numMessageIds : 16;                 // number of messages in this packet
        uint32_t acked : 1;                          // 1 if this sent packet has been acked
        uint64_t block : 1;                          // 1 if this sent packet contains a large block fragment
        uint64_t blockId : 16;                       // block id. valid only when sending block.
        uint64_t fragmentId : 16;                    // fragment id. valid only when sending block.
    };

    struct MessageReceiveQueueEntry
    {
        Message * message;
    };

    struct SendBlockData
    {
        SendBlockData() : ackedFragment( MaxFragmentsPerBlock )
        {
            Reset();
        }

        void Reset()
        {
            active = false;
            numFragments = 0;
            numAckedFragments = 0;
            messageId = 0;
            blockSize = 0;
        }

        bool active;                                                    // true if we are currently sending a large block
        int numFragments;                                               // number of fragments in the current large block being sent
        int numAckedFragments;                                          // number of acked fragments in current block being sent
        int blockSize;                                                  // send block size in bytes
        uint16_t messageId;                                             // the message id of the block being sent
        BitArray ackedFragment;                                         // has fragment n been received?
        double fragmentSendTime[MaxFragmentsPerBlock];                  // time fragment n last sent in seconds.
        uint8_t blockData[MaxBlockSize];                                // block data storage as it is received.
    };

    struct ReceiveBlockData
    {
        ReceiveBlockData() : receivedFragment( MaxFragmentsPerBlock )
        {
            Reset();
        }

        void Reset()
        {
            active = false;
            numFragments = 0;
            numReceivedFragments = 0;
            messageId = 0;
            blockSize = 0;
        }

        bool active;                                // true if we are currently receiving a large block
        int numFragments;                           // number of fragments in this block
        int numReceivedFragments;                   // number of fragments received.
        uint16_t messageId;                         // message id of block being currently received.
        uint32_t blockSize;                         // block size in bytes.
        BitArray receivedFragment;                  // has fragment n been received?
        uint8_t blockData[MaxBlockSize];            // block data for receive
    };

    void InsertAckPacketEntry( uint16_t sequence );

    void ProcessAcks( uint16_t ack, uint32_t ack_bits );

    void GetMessagesToSend( uint16_t * messageIds, int & numMessageIds );

    void AddMessagePacketEntry( const uint16_t * messageIds, int & numMessageIds, uint16_t sequence );

    void ProcessPacketMessages( const ConnectionPacket * packet );

    void ProcessMessageAck( uint16_t ack );

    void UpdateOldestUnackedMessageId();

    int CalculateMessageOverheadBits();
    
private:

    PacketFactory * m_packetFactory;                                                // packet factory for creating and destroying connection packets

    MessageFactory * m_messageFactory;                                              // message factory creates and destroys messages

    double m_time;                                                                  // current connection time

    ConnectionError m_error;                                                        // connection error level

    SequenceBuffer<SentPacketData> * m_sentPackets;                                 // sequence buffer of recently sent packets

    SequenceBuffer<ReceivedPacketData> * m_receivedPackets;                         // sequence buffer of recently received packets

    int m_messageOverheadBits;                                                      // number of bits overhead per-serialized message

    uint16_t m_sendMessageId;                                                       // id for next message added to send queue

    uint16_t m_receiveMessageId;                                                    // id for next message to be received

    uint16_t m_oldestUnackedMessageId;                                              // id for oldest unacked message in send queue

    SequenceBuffer<MessageSendQueueEntry> * m_messageSendQueue;                     // message send queue

    SequenceBuffer<MessageSentPacketEntry> * m_messageSentPackets;                  // messages in sent packets (for acks)

    SequenceBuffer<MessageReceiveQueueEntry> * m_messageReceiveQueue;               // message receive queue

    uint16_t * m_sentPacketMessageIds;                                              // array of message ids, n ids per-sent packet

    SendBlockData m_sendBlock;                                                      // data for block being sent

    ReceiveBlockData m_receiveBlock;                                                // data for block being received
};

Connection::Connection( PacketFactory & packetFactory, MessageFactory & messageFactory )
{
    assert( ( 65536 % SlidingWindowSize ) == 0 );
    assert( ( 65536 % MessageSendQueueSize ) == 0 );
    assert( ( 65536 % MessageReceiveQueueSize ) == 0 );
    
    m_packetFactory = &packetFactory;

    m_messageFactory = &messageFactory;
    
    m_error = CONNECTION_ERROR_NONE;

    m_messageOverheadBits = CalculateMessageOverheadBits();

    m_sentPackets = new SequenceBuffer<SentPacketData>( SlidingWindowSize );
    
    m_receivedPackets = new SequenceBuffer<ReceivedPacketData>( SlidingWindowSize );

    m_messageSendQueue = new SequenceBuffer<MessageSendQueueEntry>( MessageSendQueueSize );
    
    m_messageSentPackets = new SequenceBuffer<MessageSentPacketEntry>( SlidingWindowSize );
    
    m_messageReceiveQueue = new SequenceBuffer<MessageReceiveQueueEntry>( MessageReceiveQueueSize );
    
    m_sentPacketMessageIds = new uint16_t[ MaxMessagesPerPacket * MessageSendQueueSize ];

    Reset();
}

Connection::~Connection()
{
    Reset();

    assert( m_sentPackets );
    assert( m_receivedPackets );
    assert( m_messageSendQueue );
    assert( m_messageSentPackets );
    assert( m_messageReceiveQueue );
    assert( m_sentPacketMessageIds );

    delete m_sentPackets;
    delete m_receivedPackets;
    delete m_messageSendQueue;
    delete m_messageSentPackets;
    delete m_messageReceiveQueue;
    delete [] m_sentPacketMessageIds;

    m_sentPackets = NULL;
    m_receivedPackets = NULL;
    m_messageSendQueue = NULL;
    m_messageSentPackets = NULL;
    m_messageReceiveQueue = NULL;
    m_sentPacketMessageIds = NULL;
}

void Connection::Reset()
{
    m_error = CONNECTION_ERROR_NONE;

    m_time = 0.0;

    m_sentPackets->Reset();
    m_receivedPackets->Reset();

    m_sendMessageId = 0;
    m_receiveMessageId = 0;
    m_oldestUnackedMessageId = 0;

    for ( int i = 0; i < m_messageSendQueue->GetSize(); ++i )
    {
        MessageSendQueueEntry * entry = m_messageSendQueue->GetAtIndex( i );
        if ( entry && entry->message )
            entry->message->Release();
    }

    for ( int i = 0; i < m_messageReceiveQueue->GetSize(); ++i )
    {
        MessageReceiveQueueEntry * entry = m_messageReceiveQueue->GetAtIndex( i );
        if ( entry && entry->message )
            entry->message->Release();
    }

    m_messageSendQueue->Reset();
    m_messageSentPackets->Reset();
    m_messageReceiveQueue->Reset();
}

bool Connection::CanSendMessage() const
{
    return m_messageSendQueue->IsAvailable( m_sendMessageId );
}

void Connection::SendMessage( Message * message )
{
    assert( message );
    assert( CanSendMessage() );

    if ( !CanSendMessage() )
    {
        m_error = CONNECTION_ERROR_MESSAGE_SEND_QUEUE_FULL;
        message->Release();
        return;
    }

    message->AssignId( m_sendMessageId );

    MessageSendQueueEntry * entry = m_messageSendQueue->Insert( m_sendMessageId );

    assert( entry );

    if ( message->IsBlockMessage() )
    {
        // todo: block message

        assert( false );
    }
    else
    {
        entry->message = message;
        entry->block = 0;
        entry->measuredBits = 0;
        entry->timeLastSent = -1.0;

        MeasureStream measureStream( MessagePacketBudget / 2 );

        message->SerializeInternal( measureStream );

        if ( measureStream.GetError() )
        {
            m_error = CONNECTION_ERROR_MESSAGE_SERIALIZE_MEASURE_FAILED;
            message->Release();
            return;
        }

        entry->measuredBits = measureStream.GetBitsProcessed() + m_messageOverheadBits;
    }

    m_sendMessageId++;
}

Message * Connection::ReceiveMessage()
{
    if ( GetError() != CONNECTION_ERROR_NONE )
        return NULL;

    MessageReceiveQueueEntry * entry = m_messageReceiveQueue->Find( m_receiveMessageId );
    if ( !entry )
        return NULL;

    Message * message = entry->message;

    assert( message );
    assert( message->GetId() == m_receiveMessageId );

    m_messageReceiveQueue->Remove( m_receiveMessageId );

    m_receiveMessageId++;

    return message;
}

ConnectionPacket * Connection::WritePacket()
{
    if ( m_error != CONNECTION_ERROR_NONE )
        return NULL;

    ConnectionPacket * packet = (ConnectionPacket*) m_packetFactory->CreatePacket( CONNECTION_PACKET );

    if ( !packet )
        return NULL;

    packet->sequence = m_sentPackets->GetSequence();

    packet->oldest_message_id = m_oldestUnackedMessageId;

    GenerateAckBits( *m_receivedPackets, packet->ack, packet->ack_bits );

    InsertAckPacketEntry( packet->sequence );

    int numMessageIds;
    
    uint16_t messageIds[MaxMessagesPerPacket];

    GetMessagesToSend( messageIds, numMessageIds );

    AddMessagePacketEntry( messageIds, numMessageIds, packet->sequence );

    packet->numMessages = numMessageIds;

    for ( int i = 0; i < numMessageIds; ++i )
    {
        MessageSendQueueEntry * entry = m_messageSendQueue->Find( messageIds[i] );
        assert( entry && entry->message );
        packet->messages[i] = entry->message;
        entry->message->AddRef();
    }

    return packet;
}

bool Connection::ReadPacket( ConnectionPacket * packet )
{
    if ( m_error != CONNECTION_ERROR_NONE )
        return false;

    assert( packet );
    assert( packet->GetType() == CONNECTION_PACKET );

    ProcessAcks( packet->ack, packet->ack_bits );

    ProcessPacketMessages( packet );

    m_receivedPackets->Insert( packet->sequence );

    return true;
}

void Connection::AdvanceTime( double time )
{
    m_time = time;

    m_sentPackets->RemoveOldEntries();

    m_receivedPackets->RemoveOldEntries();

    m_messageSentPackets->RemoveOldEntries();
}

ConnectionError Connection::GetError() const
{
    return m_error;
}

void Connection::InsertAckPacketEntry( uint16_t sequence )
{
    SentPacketData * entry = m_sentPackets->Insert( sequence );
    
    assert( entry );

    if ( entry )
    {
        entry->acked = 0;
    }
}

void Connection::ProcessAcks( uint16_t ack, uint32_t ack_bits )
{
    for ( int i = 0; i < 32; ++i )
    {
        if ( ack_bits & 1 )
        {                    
            const uint16_t sequence = ack - i;

            SentPacketData * packetData = m_sentPackets->Find( sequence );
            
            if ( packetData && !packetData->acked )
            {
                ProcessMessageAck( sequence );

                packetData->acked = 1;
            }
        }

        ack_bits >>= 1;
    }
}

void Connection::GetMessagesToSend( uint16_t * messageIds, int & numMessageIds )
{
    numMessageIds = 0;

    if ( m_oldestUnackedMessageId == m_sendMessageId )
        return;

#if _DEBUG
    MessageSendQueueEntry * firstEntry = m_messageSendQueue->Find( m_oldestUnackedMessageId );
    assert( firstEntry );
#endif // #if _DEBUG
    
    const int GiveUpBits = 8 * 8;

    int availableBits = MessagePacketBudget * 8;

    const int messageLimit = min( MessageSendQueueSize, MessageReceiveQueueSize ) / 2;

    for ( int i = 0; i < messageLimit; ++i )
    {
        const uint16_t messageId = m_oldestUnackedMessageId + i;

        MessageSendQueueEntry * entry = m_messageSendQueue->Find( messageId );
        
        if ( entry && ( entry->timeLastSent + MessageResendRate <= m_time ) && ( availableBits - entry->measuredBits >= 0 ) )
        {
            messageIds[numMessageIds++] = messageId;
            entry->timeLastSent = m_time;
            availableBits -= entry->measuredBits;
        }

        if ( availableBits <= GiveUpBits )
            break;
        
        if ( numMessageIds == MaxMessagesPerPacket )
            break;
    }
}

void Connection::AddMessagePacketEntry( const uint16_t * messageIds, int & numMessageIds, uint16_t sequence )
{
    MessageSentPacketEntry * sentPacket = m_messageSentPackets->Insert( sequence );
    
    assert( sentPacket );

    if ( sentPacket )
    {
        sentPacket->acked = 0;
        sentPacket->block = 0;
        sentPacket->timeSent = m_time;
     
        const int sentPacketIndex = m_sentPackets->GetIndex( sequence );
     
        sentPacket->messageIds = &m_sentPacketMessageIds[sentPacketIndex*MaxMessagesPerPacket];
        sentPacket->numMessageIds = numMessageIds;
        for ( int i = 0; i < numMessageIds; ++i )
            sentPacket->messageIds[i] = messageIds[i];
    }
}

// todo: Connection::AddBlockPacketEntry

void Connection::ProcessPacketMessages( const ConnectionPacket * packet )
{
    const uint16_t minMessageId = m_receiveMessageId;
    const uint16_t maxMessageId = m_receiveMessageId + MessageReceiveQueueSize - 1;

    for ( int i = 0; i < packet->numMessages; ++i )
    {
        Message * message = packet->messages[i];

        assert( message );

        const uint16_t messageId = message->GetId();

        if ( m_messageReceiveQueue->Find( messageId ) )
            continue;

        if ( sequence_less_than( messageId, minMessageId ) )
            continue;

        if ( sequence_greater_than( messageId, maxMessageId ) )
        {
            m_error = CONNECTION_ERROR_MESSAGE_DESYNC;
            return;
        }

        MessageReceiveQueueEntry * entry = m_messageReceiveQueue->Insert( messageId );

        assert( entry );

        if ( entry )
        {
            entry->message = message;
            entry->message->AddRef();
        }
    }
}

void Connection::ProcessMessageAck( uint16_t ack )
{
    MessageSentPacketEntry * sentPacketEntry = m_messageSentPackets->Find( ack );

#if DEBUG_LOGS
    if ( !sentPacketEntry )
    {
        printf( "can't find packet %d to ack\n", ack );
        return;
    }
#endif // #if DEBUG_LOGS

    assert( !sentPacketEntry->acked );

#if DEBUG_LOGS
    printf( "ack packet %d\n", ack );
#endif // #if DEBUG_LOGS

    for ( int i = 0; i < (int) sentPacketEntry->numMessageIds; ++i )
    {
        const uint16_t messageId = sentPacketEntry->messageIds[i];

        MessageSendQueueEntry * sendQueueEntry = m_messageSendQueue->Find( messageId );
        
        if ( sendQueueEntry )
        {
            assert( sendQueueEntry->message );
            assert( sendQueueEntry->message->GetId() == messageId );

#if DEBUG_LOGS
            printf( "ack message %d\n", messageId );
#endif // #if DEBUG_LOGS

            sendQueueEntry->message->Release();

            m_messageSendQueue->Remove( messageId );
        }
    }

    UpdateOldestUnackedMessageId();
}

void Connection::UpdateOldestUnackedMessageId()
{
    const uint16_t stopMessageId = m_messageSendQueue->GetSequence();

#if DEBUG_LOGS
    uint16_t previous = m_oldestUnackedMessageId;
#endif // #if DEBUG_LOGS

    while ( true )
    {
        if ( m_oldestUnackedMessageId == stopMessageId )
            break;

        MessageSendQueueEntry * entry = m_messageSendQueue->Find( m_oldestUnackedMessageId );
        if ( entry )
            break;
       
        ++m_oldestUnackedMessageId;
    }

#if DEBUG_LOGS
    uint16_t current = m_oldestUnackedMessageId;
    if ( current != previous )
    {
        printf( "updated oldest unacked message from %d -> %d\n", previous, current );
    }
#endif // #if DEBUG_LOGS

    assert( !sequence_greater_than( m_oldestUnackedMessageId, stopMessageId ) );
}

int Connection::CalculateMessageOverheadBits()
{
    const int maxMessageType = m_messageFactory->GetNumTypes() - 1;

    const int MessageIdBits = 16;
    
    const int MessageTypeBits = protocol2::bits_required( 0, maxMessageType );

    return MessageIdBits + MessageTypeBits;
}

struct TestPacketFactory : public PacketFactory
{
    explicit TestPacketFactory() : PacketFactory( NUM_PACKET_TYPES ) {}

    Packet * Create( int type )
    {
        switch ( type )
        {
            case CONNECTION_PACKET: return new ConnectionPacket();
        }

        return NULL;
    }

    void Destroy( Packet * packet )
    {
        delete packet;
    }
};

enum MessageType
{
    TEST_MESSAGE,
    TEST_BLOCK_MESSAGE,
    NUM_MESSAGE_TYPES
};

inline int GetNumBitsForMessage( uint16_t sequence )
{
    static int messageBitsArray[] = { 1, 320, 120, 4, 256, 45, 11, 13, 101, 100, 84, 95, 203, 2, 3, 8, 512, 5, 3, 7, 50 };
    const int modulus = sizeof( messageBitsArray ) / sizeof( int );
    const int index = sequence % modulus;
    return messageBitsArray[index];
}

struct TestMessage : public Message
{
    TestMessage() : Message( TEST_MESSAGE )
    {
        sequence = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {        
        serialize_bits( stream, sequence, 16 );

        int numBits = GetNumBitsForMessage( sequence );
        int numWords = numBits / 32;
        uint32_t dummy = 0;
        for ( int i = 0; i < numWords; ++i )
            serialize_bits( stream, dummy, 32 );
        int numRemainderBits = numBits - numWords * 32;
        if ( numRemainderBits > 0 )
            serialize_bits( stream, dummy, numRemainderBits );

        serialize_check( stream, "end of test message" );

        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();

    uint16_t sequence;
};

struct TestBlockMessage : public BlockMessage
{
    TestBlockMessage() : BlockMessage( TEST_MESSAGE ) {}
};

class TestMessageFactory : public MessageFactory
{
public:

    TestMessageFactory() : MessageFactory( NUM_MESSAGE_TYPES ) {}

protected:

    Message * CreateInternal( int type )
    {
        switch ( type )
        {
            case TEST_MESSAGE:          return new TestMessage();
            case TEST_BLOCK_MESSAGE:    return new TestBlockMessage();
            default:
                return NULL;
        }
    }
};

#include <signal.h>

static volatile int quit = 0;

void interrupt_handler( int /*dummy*/ )
{
    quit = 1;
}

int main()
{
    printf( "\nreliable ordered messages\n\n" );

    TestPacketFactory packetFactory;

    TestMessageFactory messageFactory;

    Connection sender( packetFactory, messageFactory );

    Connection receiver( packetFactory, messageFactory );

    double time = 0.0;
    double deltaTime = 0.1;

    uint64_t numMessagesSent = 0;
    uint64_t numMessagesReceived = 0;

    signal( SIGINT, interrupt_handler );    

    while ( !quit )
    {
        const int messagesToSend = random_int( 0, 32 );

        for ( int i = 0; i < messagesToSend; ++i )
        {
            if ( !sender.CanSendMessage() )
                break;

            if ( rand() % 100 )
            {
                TestMessage * message = (TestMessage*) messageFactory.Create( TEST_MESSAGE );
                
                if ( message )
                {
                    message->sequence = (uint16_t) numMessagesSent;
                    
                    sender.SendMessage( message );

                    numMessagesSent++;
                }
            }
            else
            {
                TestBlockMessage * blockMessage = (TestBlockMessage*) messageFactory.Create( TEST_BLOCK_MESSAGE );

                if ( blockMessage )
                {
                    // todo: setup block such that it is of random size and contents that are a function of message id
                    // so it can be verified on the other side trivially without buffering a bunch of blocks.

                    sender.SendMessage( blockMessage );
                }
            }
        }

        ConnectionPacket * senderPacket = sender.WritePacket();
        ConnectionPacket * receiverPacket = receiver.WritePacket();

        assert( senderPacket );
        assert( receiverPacket );

        if ( ( rand() % 100 ) == 0 )
            sender.ReadPacket( receiverPacket );

        if ( ( rand() % 100 ) == 0 )
            receiver.ReadPacket( senderPacket );

        packetFactory.DestroyPacket( senderPacket );
        packetFactory.DestroyPacket( receiverPacket );

        while ( true )
        {
            Message * message = receiver.ReceiveMessage();

            if ( !message )
                break;

            assert( message->GetId() == (uint16_t) numMessagesReceived );

            switch ( message->GetType() )
            {
                case TEST_MESSAGE:
                {
                    TestMessage * testMessage = (TestMessage*) message;

                    if ( testMessage->sequence != uint16_t( numMessagesReceived ) )
                    {
                        printf( "error: received out of sequence message!\n" );
                        return 1;
                    }

                    printf( "received message %d\n", uint16_t( numMessagesReceived ) );
                }
                break;

                case TEST_BLOCK_MESSAGE:
                {
                    // ...

                    printf( "received block message %d\n", uint16_t( numMessagesReceived ) );
                }
                break;
            }

            ++numMessagesReceived;

            message->Release();
        }

        time += deltaTime;

        sender.AdvanceTime( time );

        receiver.AdvanceTime( time );

        if ( sender.GetError() || receiver.GetError() )
        {
            printf( "connection error\n" );
            return 1;
        }
    }

    printf( "\nstopped\n\n" );

    return 0;
}
