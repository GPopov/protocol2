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
const float FragmentResendRate = 0.1;

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

    int numMessages;
    Message * messages[MaxMessagesPerPacket];

    uint8_t * blockFragmentData;
    uint64_t blockMessageId : 16;
    uint64_t blockFragmentId : 16;
    uint64_t blockFragmentSize : 16;
    uint16_t blockNumFragments : 16;
    int blockMessageType;

    ConnectionPacket() : Packet( CONNECTION_PACKET )
    {
        sequence = 0;
        ack = 0;
        ack_bits = 0;
        numMessages = 0;
        blockFragmentData = NULL;
        blockMessageId = 0;
        blockFragmentId = 0;
        blockFragmentSize = 0;
        blockNumFragments = 0;
        blockMessageType = 0;
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

        if ( blockFragmentData )
        {
            delete [] blockFragmentData;
            blockFragmentData = NULL;
        }
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        ConnectionContext * context = (ConnectionContext*) stream.GetContext();

        assert( context );

        MessageFactory * messageFactory = context->messageFactory;

        const int maxMessageType = messageFactory->GetNumTypes() - 1;

        // serialize ack system

        serialize_bits( stream, sequence, 16 );

        serialize_bits( stream, ack, 16 );

        serialize_bits( stream, ack_bits, 32 );

        // serialize messages

        bool hasMessages = numMessages != 0;

        serialize_bool( stream, hasMessages );

        if ( hasMessages )
        {
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

                    if ( !messages[i] )
                        return false;

                    messages[i]->AssignId( messageIds[i] );
                }

                assert( messages[i] );

                if ( !messages[i]->SerializeInternal( stream ) )
                    return false;
            }
        }

        // serialize block fragment

        bool hasFragment = Stream::IsWriting && blockFragmentData;

        serialize_bool( stream, hasFragment );

        if ( hasFragment )
        {
            serialize_bits( stream, blockMessageId, 16 );

            serialize_int( stream, blockNumFragments, 1, MaxFragmentsPerBlock );

            serialize_int( stream, blockFragmentId, 0, blockNumFragments - 1 );

            serialize_int( stream, blockFragmentSize, 1, BlockFragmentSize );

            serialize_bytes( stream, blockFragmentData, blockFragmentSize );

            if ( blockFragmentId == 0 )
            {
                serialize_int( stream, blockMessageType, 0, maxMessageType );
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
    CONNECTION_ERROR_MESSAGE_OUT_OF_MEMORY
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
        uint64_t block : 1;                          // 1 if this sent packet contains a block fragment
        uint64_t blockMessageId : 16;                // block id. valid only when sending block.
        uint64_t blockFragmentId : 16;               // fragment id. valid only when sending block.
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
            blockMessageId = 0;
            blockSize = 0;
        }

        bool active;                                                    // true if we are currently sending a block
        int numFragments;                                               // number of fragments in the current block being sent
        int numAckedFragments;                                          // number of acked fragments in current block being sent
        int blockSize;                                                  // send block size in bytes
        uint16_t blockMessageId;                                        // the message id of the block being sent
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
            messageType = 0;
            blockSize = 0;
        }

        bool active;                                                    // true if we are currently receiving a block
        int numFragments;                                               // number of fragments in this block
        int numReceivedFragments;                                       // number of fragments received.
        uint16_t messageId;                                             // message id of block being currently received.
        int messageType;                                                // message type of the block being received.
        uint32_t blockSize;                                             // block size in bytes.
        BitArray receivedFragment;                                      // has fragment n been received?
        uint8_t blockData[MaxBlockSize];                                // block data for receive
    };

    void InsertAckPacketEntry( uint16_t sequence );

    void ProcessAcks( uint16_t ack, uint32_t ack_bits );

    bool HasMessagesToSend();

    void GetMessagesToSend( uint16_t * messageIds, int & numMessageIds );

    void AddMessagesToPacket( const uint16_t * messageIds, int numMessageIds, ConnectionPacket * packet );

    void AddMessagePacketEntry( const uint16_t * messageIds, int numMessageIds, uint16_t sequence );

    void ProcessPacketMessages( const ConnectionPacket * packet );

    void ProcessMessageAck( uint16_t ack );

    void UpdateOldestUnackedMessageId();

    int CalculateMessageOverheadBits();

    bool SendingBlockMessage();

    uint8_t * GetFragmentToSend( uint16_t & messageId, uint16_t & fragmentId, int & fragmentBytes, int & numFragments, int & messageType );

    void AddFragmentToPacket( uint16_t messageId, uint16_t fragmentId, uint8_t * fragmentData, int fragmentSize, int numFragments, int messageType, ConnectionPacket * packet );

    void AddFragmentPacketEntry( uint16_t messageId, uint16_t fragmentId, uint16_t sequence );

    void ProcessPacketFragment( const ConnectionPacket * packet );

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

    m_sendBlock.Reset();
    m_receiveBlock.Reset();
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

    entry->block = message->IsBlockMessage();
    entry->message = message;
    entry->measuredBits = 0;
    entry->timeLastSent = -1.0;

    if ( message->IsBlockMessage() )
    {
        BlockMessage * blockMessage = (BlockMessage*) message;

        assert( blockMessage->GetBlockSize() > 0 );
        assert( blockMessage->GetBlockSize() <= MaxBlockSize );
    }
    else
    {
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

    GenerateAckBits( *m_receivedPackets, packet->ack, packet->ack_bits );

    InsertAckPacketEntry( packet->sequence );

    int numMessageIds = 0;
    uint16_t messageIds[MaxMessagesPerPacket];

    if ( HasMessagesToSend() )
    {
        if ( SendingBlockMessage() )
        {
            uint16_t messageId;
            uint16_t fragmentId;
            int fragmentBytes;
            int numFragments;
            int messageType;

            uint8_t * fragmentData = GetFragmentToSend( messageId, fragmentId, fragmentBytes, numFragments, messageType );

            if ( fragmentData )
            {
                AddFragmentToPacket( messageId, fragmentId, fragmentData, fragmentBytes, numFragments, messageType, packet );

                AddFragmentPacketEntry( messageId, fragmentId, packet->sequence );
            }
        }
        else
        {
            GetMessagesToSend( messageIds, numMessageIds );

            AddMessagesToPacket( messageIds, numMessageIds, packet );

            AddMessagePacketEntry( messageIds, numMessageIds, packet->sequence );
        }
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

    ProcessPacketFragment( packet );

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

bool Connection::HasMessagesToSend()
{
    return m_oldestUnackedMessageId != m_sendMessageId;
}

void Connection::GetMessagesToSend( uint16_t * messageIds, int & numMessageIds )
{
    assert( HasMessagesToSend() );

    numMessageIds = 0;

    const int GiveUpBits = 8 * 8;

    int availableBits = MessagePacketBudget * 8;

    const int messageLimit = min( MessageSendQueueSize, MessageReceiveQueueSize ) / 2;

    for ( int i = 0; i < messageLimit; ++i )
    {
        const uint16_t messageId = m_oldestUnackedMessageId + i;

        MessageSendQueueEntry * entry = m_messageSendQueue->Find( messageId );

        if ( !entry )
            continue;

        if ( entry->block )
            break;
        
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

void Connection::AddMessagesToPacket( const uint16_t * messageIds, int numMessageIds, ConnectionPacket * packet )
{
    assert( packet );

    packet->numMessages = numMessageIds;

    for ( int i = 0; i < numMessageIds; ++i )
    {
        MessageSendQueueEntry * entry = m_messageSendQueue->Find( messageIds[i] );
        assert( entry && entry->message );
        packet->messages[i] = entry->message;
        entry->message->AddRef();
    }
}

void Connection::AddMessagePacketEntry( const uint16_t * messageIds, int numMessageIds, uint16_t sequence )
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

    if ( !sentPacketEntry )
        return;

    assert( !sentPacketEntry->acked );

    for ( int i = 0; i < (int) sentPacketEntry->numMessageIds; ++i )
    {
        const uint16_t messageId = sentPacketEntry->messageIds[i];

        MessageSendQueueEntry * sendQueueEntry = m_messageSendQueue->Find( messageId );
        
        if ( sendQueueEntry )
        {
            assert( sendQueueEntry->message );
            assert( sendQueueEntry->message->GetId() == messageId );

            sendQueueEntry->message->Release();

            m_messageSendQueue->Remove( messageId );

            UpdateOldestUnackedMessageId();
        }
    }

    if ( sentPacketEntry->block && m_sendBlock.active && m_sendBlock.blockMessageId == sentPacketEntry->blockMessageId )
    {        
        const int messageId = sentPacketEntry->blockMessageId;
        const int fragmentId = sentPacketEntry->blockFragmentId;

        if ( !m_sendBlock.ackedFragment.GetBit( fragmentId ) )
        {
            m_sendBlock.ackedFragment.SetBit( fragmentId );

            m_sendBlock.numAckedFragments++;

            if ( m_sendBlock.numAckedFragments == m_sendBlock.numFragments )
            {
                m_sendBlock.active = false;

                MessageSendQueueEntry * sendQueueEntry = m_messageSendQueue->Find( messageId );

                assert( sendQueueEntry );

                sendQueueEntry->message->Release();

                m_messageSendQueue->Remove( messageId );

                UpdateOldestUnackedMessageId();
            }
        }
    }
}

void Connection::UpdateOldestUnackedMessageId()
{
    const uint16_t stopMessageId = m_messageSendQueue->GetSequence();

    while ( true )
    {
        if ( m_oldestUnackedMessageId == stopMessageId )
            break;

        MessageSendQueueEntry * entry = m_messageSendQueue->Find( m_oldestUnackedMessageId );
        if ( entry )
            break;
       
        ++m_oldestUnackedMessageId;
    }

    assert( !sequence_greater_than( m_oldestUnackedMessageId, stopMessageId ) );
}

int Connection::CalculateMessageOverheadBits()
{
    const int maxMessageType = m_messageFactory->GetNumTypes() - 1;

    const int MessageIdBits = 16;
    
    const int MessageTypeBits = protocol2::bits_required( 0, maxMessageType );

    return MessageIdBits + MessageTypeBits;
}

bool Connection::SendingBlockMessage()
{
    assert( HasMessagesToSend() );

    MessageSendQueueEntry * entry = m_messageSendQueue->Find( m_oldestUnackedMessageId );

    return entry->block;
}

uint8_t * Connection::GetFragmentToSend( uint16_t & messageId, uint16_t & fragmentId, int & fragmentBytes, int & numFragments, int & messageType )
{
    MessageSendQueueEntry * entry = m_messageSendQueue->Find( m_oldestUnackedMessageId );

    assert( entry );
    assert( entry->block );

    BlockMessage * blockMessage = (BlockMessage*) entry->message;

    assert( blockMessage );

    messageId = blockMessage->GetId();

    const int blockSize = blockMessage->GetBlockSize();

    if ( !m_sendBlock.active )
    {
        // start sending this block

        m_sendBlock.active = true;
        m_sendBlock.blockSize = blockSize;
        m_sendBlock.blockMessageId = messageId;
        m_sendBlock.numFragments = (int) ceil( blockSize / float( BlockFragmentSize ) );
        m_sendBlock.numAckedFragments = 0;

        assert( m_sendBlock.numFragments > 0 );
        assert( m_sendBlock.numFragments <= MaxFragmentsPerBlock );

        m_sendBlock.ackedFragment.Clear();

        for ( int i = 0; i < MaxFragmentsPerBlock; ++i )
            m_sendBlock.fragmentSendTime[i] = -1.0;
    }

    numFragments = m_sendBlock.numFragments;

    // find the next fragment to send (there may not be one)

    fragmentId = 0xFFFF;

    for ( int i = 0; i < m_sendBlock.numFragments; ++i )
    {
        if ( !m_sendBlock.ackedFragment.GetBit( i ) && m_sendBlock.fragmentSendTime[i] + FragmentResendRate < m_time )
        {
            fragmentId = uint16_t( i );
            break;
        }
    }

    if ( fragmentId == 0xFFFF )
        return NULL;

    // allocate and return a copy of the fragment data

    messageType = blockMessage->GetType();

    fragmentBytes = BlockFragmentSize;
    
    const int fragmentRemainder = blockSize % BlockFragmentSize;

    if ( fragmentRemainder && fragmentId == m_sendBlock.numFragments - 1 )
        fragmentBytes = fragmentRemainder;

    uint8_t * fragmentData = new uint8_t[fragmentBytes];

    if ( fragmentData )
    {
        memcpy( fragmentData, blockMessage->GetBlockData() + fragmentId * BlockFragmentSize, fragmentBytes );

        m_sendBlock.fragmentSendTime[fragmentId] = m_time;
    }

    return fragmentData;
}

void Connection::AddFragmentToPacket( uint16_t messageId, uint16_t fragmentId, uint8_t * fragmentData, int fragmentSize, int numFragments, int messageType, ConnectionPacket * packet )
{
    assert( packet );

    packet->blockFragmentData = fragmentData;
    packet->blockMessageId = messageId;
    packet->blockFragmentId = fragmentId;
    packet->blockFragmentSize = fragmentSize;
    packet->blockNumFragments = numFragments;
    packet->blockMessageType = messageType;
}

void Connection::AddFragmentPacketEntry( uint16_t messageId, uint16_t fragmentId, uint16_t sequence )
{
    MessageSentPacketEntry * sentPacket = m_messageSentPackets->Insert( sequence );
    
    assert( sentPacket );

    if ( sentPacket )
    {
        sentPacket->numMessageIds = 0;
        sentPacket->messageIds = NULL;
        sentPacket->timeSent = m_time;
        sentPacket->acked = 0;
        sentPacket->block = 1;
        sentPacket->blockMessageId = messageId;
        sentPacket->blockFragmentId = fragmentId;
    }
}

void Connection::ProcessPacketFragment( const ConnectionPacket * packet )
{  
    if ( packet->blockFragmentData )
    {
        const uint16_t messageId = packet->blockMessageId;
        const uint16_t expectedMessageId = m_messageReceiveQueue->GetSequence();

        // todo: may be necessary to bring back ignore old message, but if new message id, return false

        if ( messageId != expectedMessageId )
            return;

        if ( !m_receiveBlock.active )
        {
            const int numFragments = packet->blockNumFragments;

            assert( numFragments >= 0 );
            assert( numFragments <= MaxFragmentsPerBlock );

            m_receiveBlock.active = true;
            m_receiveBlock.numFragments = numFragments;
            m_receiveBlock.numReceivedFragments = 0;
            m_receiveBlock.messageId = messageId;
            m_receiveBlock.blockSize = 0;
            m_receiveBlock.receivedFragment.Clear();
        }

        // todo: validate fragment

        // receive the fragment

        const uint16_t fragmentId = packet->blockFragmentId;

        if ( !m_receiveBlock.receivedFragment.GetBit( fragmentId ) )
        {
            printf( "received fragment %d\n", fragmentId );

            m_receiveBlock.receivedFragment.SetBit( fragmentId );

            const int fragmentBytes = packet->blockFragmentSize;

            memcpy( m_receiveBlock.blockData + fragmentId * BlockFragmentSize, packet->blockFragmentData, fragmentBytes );

            if ( fragmentId == 0 )
            {
                m_receiveBlock.messageType = packet->blockMessageType;
            }

            if ( fragmentId == m_receiveBlock.numFragments - 1 )
            {
                m_receiveBlock.blockSize = ( m_receiveBlock.numFragments - 1 ) * BlockFragmentSize + fragmentBytes;

                assert( m_receiveBlock.blockSize >= 0 && m_receiveBlock.blockSize <= MaxBlockSize );
            }

            m_receiveBlock.numReceivedFragments++;

            if ( m_receiveBlock.numReceivedFragments == m_receiveBlock.numFragments )
            {
                Message * message = m_messageFactory->Create( m_receiveBlock.messageType );

                assert( message );

                if ( !message || !message->IsBlockMessage() )
                {
                    m_error = CONNECTION_ERROR_MESSAGE_DESYNC;
                    return;
                }

                BlockMessage * blockMessage = (BlockMessage*) message;

                uint8_t * blockData = new uint8_t[m_receiveBlock.blockSize];

                // todo: out of memory error

                assert( blockData );

                blockMessage->Connect( blockData, m_receiveBlock.blockSize );

                blockMessage->AssignId( messageId );

                m_receiveBlock.active = false;

                MessageReceiveQueueEntry * entry = m_messageReceiveQueue->Insert( messageId );

                assert( entry );

                entry->message = blockMessage;
            }
        }
    }
}

#if 0

        if ( data->largeBlock )
        {
            /*
                Large block mode.
                This packet includes a fragment for the large block currently 
                being received. Only one large block is sent at a time.
            */

            if ( !m_receiveLargeBlock.active )
            {
                const uint16_t expectedBlockId = m_receiveQueue->GetSequence();

                if ( data->blockId != expectedBlockId )
                {
//                        printf( "unexpected large block id\n" );
                    return false;
                }

                const int numFragments = (int) ceil( data->blockSize / (float)m_config.blockFragmentSize );

                CORE_ASSERT( numFragments >= 0 );
                CORE_ASSERT( numFragments <= m_maxBlockFragments );

                if ( numFragments < 0 || numFragments > m_maxBlockFragments )
                {
                    //printf( "large block num fragments outside of range\n" );
                    return false;
                }

//                    printf( "receiving large block %d (%d bytes)\n", data->blockId, data->blockSize );

                m_receiveLargeBlock.active = true;
                m_receiveLargeBlock.numFragments = numFragments;
                m_receiveLargeBlock.numReceivedFragments = 0;
                m_receiveLargeBlock.blockId = data->blockId;
                m_receiveLargeBlock.blockSize = data->blockSize;

                CORE_ASSERT( m_config.largeBlockAllocator );
                uint8_t * blockData = (uint8_t*) m_config.largeBlockAllocator->Allocate( data->blockSize );
                m_receiveLargeBlock.block.Connect( *m_config.largeBlockAllocator, blockData, data->blockSize );
                
                m_receiveLargeBlock.received_fragment->Clear();
            }

            CORE_ASSERT( m_receiveLargeBlock.active );

            if ( data->blockId != m_receiveLargeBlock.blockId )
            {
//                    printf( "unexpected large block id. got %d but was expecting %d\n", data->blockId, m_receiveLargeBlock.blockId );
                return false;
            }

            CORE_ASSERT( data->blockId == m_receiveLargeBlock.blockId );
            CORE_ASSERT( data->blockSize == m_receiveLargeBlock.blockSize );
            CORE_ASSERT( data->fragmentId < m_receiveLargeBlock.numFragments );

            if ( data->blockId != m_receiveLargeBlock.blockId )
            {
//                    printf( "recieve large block id mismatch. got %d but was expecting %d\n", data->blockId, m_receiveLargeBlock.blockId );
                return false;
            }

            if ( data->blockSize != m_receiveLargeBlock.blockSize )
            {
//                    printf( "large block size mismatch. got %d but was expecting %d\n", data->blockSize, m_receiveLargeBlock.blockSize );
                return false;
            }

            if ( data->fragmentId >= m_receiveLargeBlock.numFragments )
            {
//                    printf( "large block fragment out of bounds.\n" );
                return false;
            }

            if ( !m_receiveLargeBlock.received_fragment->GetBit( data->fragmentId ) )
            {
/*
                printf( "received fragment " << data->fragmentId << " of large block " << m_receiveLargeBlock.blockId
                     << " (" << m_receiveLargeBlock.numReceivedFragments+1 << "/" << m_receiveLargeBlock.numFragments << ")" << endl;
                     */

                m_receiveLargeBlock.received_fragment->SetBit( data->fragmentId );

                Block & block = m_receiveLargeBlock.block;

                int fragmentBytes = m_config.blockFragmentSize;
                int fragmentRemainder = block.GetSize() % m_config.blockFragmentSize;
                if ( fragmentRemainder && data->fragmentId == m_receiveLargeBlock.numFragments - 1 )
                    fragmentBytes = fragmentRemainder;

//                    printf( "fragment bytes = %d\n", fragmentBytes );

                CORE_ASSERT( fragmentBytes >= 0 );
                CORE_ASSERT( fragmentBytes <= m_config.blockFragmentSize );
                uint8_t * src = data->fragment;
                uint8_t * dst = &( block.GetData()[data->fragmentId*m_config.blockFragmentSize] );
                memcpy( dst, src, fragmentBytes );

                m_receiveLargeBlock.numReceivedFragments++;

                if ( m_receiveLargeBlock.numReceivedFragments == m_receiveLargeBlock.numFragments )
                {
//                        printf( "received large block %d (%d bytes)\n", m_receiveLargeBlock.blockId, m_receiveLargeBlock.block->size() );

                    auto blockMessage = (BlockMessage*) m_config.messageFactory->Create( BlockMessageType );
                    CORE_ASSERT( blockMessage );
                    blockMessage->Connect( m_receiveLargeBlock.block );
                    blockMessage->SetId( m_receiveLargeBlock.blockId );

                    auto entry = m_receiveQueue->Insert( m_receiveLargeBlock.blockId );
                    CORE_ASSERT( entry );
                    entry->message = blockMessage;

                    m_receiveLargeBlock.active = false;

                    CORE_ASSERT( !m_receiveLargeBlock.block.IsValid() );
                }
            }
        }

#endif // #if 0

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
    TestBlockMessage() : BlockMessage( TEST_BLOCK_MESSAGE ) {}
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
                    const int blockSize = 1 + ( int( numMessagesSent ) * 33 ) % MaxBlockSize;

                    uint8_t * blockData = new uint8_t[blockSize];

                    for ( int j = 0; j < blockSize; ++j )
                        blockData[j] = uint8_t( numMessagesSent + j );

                    blockMessage->Connect( blockData, blockSize );

                    sender.SendMessage( blockMessage );

                    numMessagesSent++;
                }
            }
        }

        ConnectionPacket * senderPacket = sender.WritePacket();
        ConnectionPacket * receiverPacket = receiver.WritePacket();

        assert( senderPacket );
        assert( receiverPacket );

        // todo: need to actually serialize these packets and put them through the simulator

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
                        printf( "error: received out of sequence message. expected %d, got %d\n", uint16_t( numMessagesReceived ), testMessage->sequence );
                        return 1;
                    }

                    printf( "received message %d\n", uint16_t( numMessagesReceived ) );
                }
                break;

                case TEST_BLOCK_MESSAGE:
                {
                    TestBlockMessage * blockMessage = (TestBlockMessage*) message;

                    const uint8_t * blockData = blockMessage->GetBlockData();

                    const int blockSize = blockMessage->GetBlockSize();

                    const int expectedBlockSize = 1 + ( int( numMessagesReceived ) * 33 ) % MaxBlockSize;

                    if ( blockSize  != expectedBlockSize )
                    {
                        printf( "error: block size mismatch. expected %d, got %d\n", expectedBlockSize, blockSize );
                        return 1;
                    }

                    for ( int i = 0; i < blockSize; ++i )
                    {
                        if ( blockData[i] != uint8_t( numMessagesReceived + i ) )
                        {
                            printf( "error: block data mismatch. expected %d, but blockData[%d] = %d\n", uint8_t( numMessagesReceived + i ), i, blockData[i] );
                            return 1;
                        }
                    }

                    printf( "received block %d\n", uint16_t( numMessagesReceived ) );
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
