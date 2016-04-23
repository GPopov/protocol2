/*
    Example source code for "Reliable Packets over UDP"

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

// todo: this one is work in progress and not finished yet

#define PROTOCOL2_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"
#include <stdio.h>
#include <stdib.h>
#include <time.h>

//#define SOAK_TEST 1                // uncomment this line to loop forever and soak. it's the only way to be really sure it's working!

#if !SOAK_TEST
const int NumIterations = 16;
#endif // #if !SOAK_TEST

const int MaxPacketsPerIteration = 8;

const uint32_t MaxPacketSize = 1024;

const uint32_t ProtocolId = 0x22446688;

enum TestPacketTypes
{
    TEST_PACKET_A,
    TEST_PACKET_B,
    TEST_PACKET_C,
    TEST_PACKET_NUM_TYPES
};

struct Vector
{
    float x,y,z;
};

inline int random_int( int min, int max )
{
    assert( max > min );
    int result = min + rand() % ( max - min + 1 );
    assert( result >= min );
    assert( result <= max );
    return result;
}

inline float random_float( float min, float max )
{
    const int res = 10000000;
    double scale = ( rand() % res ) / double( res - 1 );
    return (float) ( min + (double) ( max - min ) * scale );
}

struct TestPacketA : public protocol2::Packet
{
    int a,b,c;

    TestPacketA() : Packet( TEST_PACKET_A )
    {
        a = random_int( -10, +10 );
        b = random_int( -20, +20 );
        c = random_int( -30, +30 );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_int( stream, a, -10, 10 );
        serialize_int( stream, b, -20, 20 );
        serialize_int( stream, c, -30, 30 );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();

    bool operator == ( const TestPacketA & other ) const
    {
        return a == other.a && b == other.b && c == other.c;
    }

    bool operator != ( const TestPacketA & other ) const
    {
        return ! ( *this == other );
    }
};

static const int MaxItems = 16;

struct TestPacketB : public protocol2::Packet
{
    int numItems;
    int items[MaxItems];

    TestPacketB() : Packet( TEST_PACKET_B )
    {
        numItems = random_int( 0, MaxItems );
        for ( int i = 0; i < numItems; ++i )
            items[i] = random_int( -100, +100 );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_int( stream, numItems, 0, MaxItems );
        for ( int i = 0; i < numItems; ++i )
            serialize_int( stream, items[i], -100, +100 );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();

    bool operator == ( const TestPacketB & other ) const
    {
        if ( numItems != other.numItems )
            return false;
        for ( int i = 0; i < numItems; ++i )
        {
            if ( items[i] != other.items[i] )
                return false;
        }
        return true;
    }

    bool operator != ( const TestPacketB & other ) const
    {
        return ! ( *this == other );
    }
};

struct TestPacketC : public protocol2::Packet
{
    Vector position;
    Vector velocity;

    TestPacketC() : Packet( TEST_PACKET_C )
    {
        position.x = random_float( -1000, +1000 );
        position.y = random_float( -1000, +1000 );
        position.z = random_float( -1000, +1000 );

        if ( rand() % 2 )
        {
            velocity.x = random_float( -100, +100 );
            velocity.y = random_float( -100, +100 );
            velocity.z = random_float( -100, +100 );
        }
        else
        {
            velocity.x = 0.0f;
            velocity.y = 0.0f;
            velocity.z = 0.0f;
        }
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_float( stream, position.x );
        serialize_float( stream, position.y );
        serialize_float( stream, position.z );

        bool at_rest = Stream::IsWriting && velocity.x == 0.0f && velocity.y == 0.0f && velocity.z == 0.0f;

        serialize_bool( stream, at_rest );

        if ( !at_rest )
        {
            serialize_float( stream, velocity.x );
            serialize_float( stream, velocity.y );
            serialize_float( stream, velocity.z );
        }
        else
        {
            if ( Stream::IsReading )
            {
                velocity.x = 0.0f;
                velocity.y = 0.0f;
                velocity.z = 0.0f;
            }
        }

        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();

    bool operator == ( const TestPacketC & other )
    {
        return position.x == other.position.x &&
               position.y == other.position.y &&
               position.z == other.position.z &&
               velocity.x == other.velocity.x &&
               velocity.y == other.velocity.y &&
               velocity.z == other.velocity.z;
    }
};

struct TestPacketFactory : public protocol2::PacketFactory
{
    TestPacketFactory() : PacketFactory( TEST_PACKET_NUM_TYPES ) {}

    protocol2::Packet* CreateInternal( int type )
    {
        switch ( type )
        {
            case TEST_PACKET_A: return new TestPacketA();
            case TEST_PACKET_B: return new TestPacketB();
            case TEST_PACKET_C: return new TestPacketC();
        }
        return NULL;
    }
};

bool CheckPacketsAreIdentical( protocol2::Packet *p1, protocol2::Packet *p2 )
{
    assert( p1 );
    assert( p2 );

    if ( p1->GetType() != p2->GetType() )
        return false;

    switch ( p1->GetType() )
    {
        case TEST_PACKET_A:     return *((TestPacketA*)p1) == *((TestPacketA*)p2);
        case TEST_PACKET_B:     return *((TestPacketB*)p1) == *((TestPacketB*)p2);
        case TEST_PACKET_C:     return *((TestPacketC*)p1) == *((TestPacketC*)p2);
        default:
            return false;
    }
}

struct AggregatePacketHeader : public protocol2::Object
{
    // todo: ack info goes in here

    template <typename Stream> bool Serialize( Stream & stream )
    {
        int test = 10;
        serialize_int( stream, test, 0, 20 );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct PacketHeader : public protocol2::Object
{
    bool reliable;
    uint16_t sequence;

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bool( stream, reliable );
        serialize_bits( stream, sequence, 16 );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

TestPacketFactory packetFactory;

class Node
{
    uint16_t send_sequence_reliable;
    uint16_t send_sequence_unreliable;

    uint16_t receive_sequence_reliable;
    uint16_t receive_sequence_unreliable;

    // todo: send queue

    // todo: receive queue

public:

    Node()
    {
        send_sequence_reliable = 0;
        send_sequence_unreliable = 0;
        receive_sequence_reliable = 0xFFFF;                    // todo: do we need bounds instead, eg. oldest packet in receive queue, newest packet in recv queue? probably.
        receive_sequence_unreliable = 0xFFFF;
    }

    void SendPacket( protocol2::Packet *packet, bool reliable )
    {
        assert( packet );

        // todo: serialize write the packet

        // todo: queue the packet data to be sent (eg. uint8_t*, int packet_size, with reliable flag and corresponding sequence number)

        packetFactory.DestroyPacket( packet );
    }

    void ProcessAcks( const uint16_t *acks, int numAcks )
    {
        // todo: remove any reliable packets in the send queue if they match an ack sequence
    }

    protocol2::Packet* GenerateAggregatePacket( double t )
    {
        // todo: work out acks that should be sent. this depends on receiver. it seems we must combine the receiver and sender in this case

        // todo: generate aggregate packet header (acks), also I think we need to send the sequence # of the oldest unacked packet

        // todo: queue up unreliable packets first (assumed time critical)

        // todo: select n most reliable packets next, use minimum resend time, maybe sort in order of oldest -> newest

        // todo: actually build the aggregate packet and return it to caller

        return NULL;
    }

    void ProcessAggregatePacket( protocol2::Packet *packet )
    {
        assert( packet );

        // todo: process the aggregate packet. 

        // 1. get the acks out of the aggregate packet header, call "ProcessAcks"

        // 2. process each of the packets in turn, and add them to the receive queue (if possible, they must be in range and we cannot go too far ahead of oldest unacked packet)

        // 3. if a packet is received, add it to the ack queue (rolling window of received, true/false). if it cannot fit in send queue, don't add to ack queue

        // 4. as each packet is acked, need to update oldest unacked (eg. slide from left -> right advance to move it forward), *or* do it once at end.
    }
};

int main()
{
    srand( time( NULL ) );

    uint16_t sequence = 0;

    Node a;
    Node b;

    AggregatePacketHeader aggregateReadHeader;
    AggregatePacketHeader aggregateWriteHeader;

    PacketHeader *readPacketHeaders[MaxPacketsPerIteration];
    PacketHeader *writePacketHeaders[MaxPacketsPerIteration];

    for ( int i = 0; i < MaxPacketsPerIteration; ++i )
    {
        readPacketHeaders[i] = new PacketHeader();
        writePacketHeaders[i] = new PacketHeader();
    }

#if !SOAK_TEST
    for ( int i = 0; i < NumIterations; ++i )
#else // #if !SOAK_TEST
    for ( uint32_t i = 0; ; ++i )
#endif // #if !SOAK_TEST
    {
        int numReadPackets = 0;
        int numWritePackets = 0;
        
        protocol2::Packet *readPackets[MaxPacketsPerIteration];
        protocol2::Packet *writePackets[MaxPacketsPerIteration];

        printf( "==============================================================\n" );
        printf( "iteration %d\n", i );

        // todo: create one big packet to send unreliably (this represents state in typical protocol)

        // ...

        // create an array of packets to be sent reliably

        numWritePackets = random_int( 0, MaxPacketsPerIteration );

        printf( "creating %d reliable packets\n", numWritePackets );

        for ( int j = 0; j < numWritePackets; ++j )
        {
            const int packetType = rand() % TEST_PACKET_NUM_TYPES;

            printf( "%d: created packet %d [%d]\n", j, sequence, packetType );

            writePackets[j] = packetFactory.CreatePacket( packetType );

            assert( writePackets[j] );

            writePacketHeaders[j]->reliable = true;
            writePacketHeaders[j]->sequence = sequence++;
        }

        // combine packets together into one aggregate on-the-wire packet

        uint8_t writeBuffer[MaxPacketSize];

        int numPacketsActuallyWritten = 0;

        const int bytesWritten = protocol2::WriteAggregatePacket( numWritePackets,
                                                                  writePackets, 
                                                                  packetFactory.GetNumTypes(), 
                                                                  writeBuffer, 
                                                                  MaxPacketSize, 
                                                                  ProtocolId, 
                                                                  numPacketsActuallyWritten,
                                                                  &aggregateWriteHeader,
                                                                  (protocol2::Object**) writePacketHeaders );

        bool error = false;

        if ( bytesWritten > 0 )
        {
            printf( "wrote aggregate packet (%d bytes)\n", bytesWritten );

            assert( numPacketsActuallyWritten == numWritePackets );
        }
        else
        {
            printf( "write aggregate packet failed\n" );
            
            error = true;

            goto cleanup;
        }

        // todo: actually send this packet over the wire

        // todo: implement sender and receiver

        // todo: setup 

        // read individual packets from the aggregate on-the-wire packet

        {
            int bytesToRead = bytesWritten;

            uint8_t readBuffer[MaxPacketSize];

            memset( readBuffer, 0, MaxPacketSize );
            memcpy( readBuffer, writeBuffer, bytesWritten );

            int readError = 0;

            printf( "reading aggregate packet (%d bytes)\n", bytesToRead );

            ReadAggregatePacket( MaxPacketsPerIteration, 
                                 readPackets, 
                                 packetFactory, 
                                 readBuffer, 
                                 bytesWritten, 
                                 ProtocolId, 
                                 numReadPackets, 
                                 &aggregateReadHeader, 
                                 (protocol2::Object**) readPacketHeaders, 
                                 &readError );

            if ( readError != PROTOCOL2_ERROR_NONE )
            {
                printf( "read packet error: %s\n", protocol2::GetErrorString( readError ) );
                error = true;
                goto cleanup;
            }

            printf( "num packets read: %d\n", numReadPackets );

            // verify that packets read from the aggregate packet exactly match the packets written to it

            assert( numReadPackets == numWritePackets );

            for ( int i = 0; i < numReadPackets; ++i )
            {
                assert( readPackets[i] );

                printf( "%d: read packet %d [%d]\n", i, readPacketHeaders[i]->sequence, readPackets[i]->GetType() );

                if ( readPacketHeaders[i]->sequence != writePacketHeaders[i]->sequence )
                {
                    printf( "read packet header is not the same as written packet header. something wrong with serialize function?\n" );
                    error = true;
                    goto cleanup;
                }

                if ( !CheckPacketsAreIdentical( readPackets[i], writePackets[i] ) )
                {
                    printf( "read packet is not the same as written packet. something wrong with serialize function?\n" );
                    error = true;
                    goto cleanup;
                }
            }

            if ( numReadPackets > 0 )
                printf( "read packets match written packets\n" );
        }

cleanup:

        // clean up packets for this iteration

        for ( int j = 0; j < numWritePackets; ++j )
        {
            packetFactory.DestroyPacket( writePackets[j] );
        }

        for ( int j = 0; j < numReadPackets; ++j )
        {
            packetFactory.DestroyPacket( readPackets[j] );
        }

        printf( "==============================================================\n\n" );

        // has there been an error? stop.

        if ( error )
            break;
    }

    for ( int i = 0; i < MaxPacketsPerIteration; ++i )
    {
        delete readPacketHeaders[i];
        delete writePacketHeaders[i];
    }

    return 0;
}
