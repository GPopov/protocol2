/*
    Example source code for "Packet Fragmentation and Reassembly"

    http://gafferongames.com/building-a-game-network-protocol/packet-fragmentation-and-reassembly

    This software is in the public domain. Where that dedication is not recognized, 
    you are granted a perpetual, irrevocable license to copy, distribute, and modify this file as you see fit.
*/

#define PROTOCOL2_IMPLEMENTATION

#include "protocol2.h"
#include <stdio.h>
#include <time.h>

const int NumIterations = 32;
const int PacketBufferSize = 32;                        // size of packet buffer, eg. number of historical packets for which we can buffer fragments
const int MaxFragmentSize = 1024;                       // maximum size of a packet fragment
const int MaxFragmentsPerPacket = 256;                  // maximum number of fragments per-packet
const int MaxPacketSize = MaxFragmentSize * MaxFragmentsPerPacket;

const uint32_t ProtocolId = 0x55667788;

struct Fragment
{
    int size;                                           // fragment size in bytes (zero if fragment not received)
    uint8_t *data;                                      // pointer to fragment data for this fragment (dynamically allocated, NULL if not allocated)
};

struct PacketBufferEntry
{
    uint32_t sequence : 16;                             // packet sequence number
    uint32_t totalFragments : 8;                        // total number of fragments for this packet
    uint32_t receivedFragments : 8;                     // number of received fragments so far
    Fragment fragments[MaxFragmentsPerPacket];          // fragment data for this packet
};

struct PacketData
{
    int size;
    uint8_t *data;
};

struct PacketBuffer
{
    PacketBuffer() { memset( this, 0, sizeof( PacketBuffer ) ); }

    uint16_t currentSequence;                           // sequence number of most recent packet in buffer

    int numFragments;                                   // number of fragments currently buffered

    bool valid[PacketBufferSize];                       // true if there is a valid buffered packet entry at this index

    PacketBufferEntry entries[PacketBufferSize];        // buffered packets in range [ current_sequence - PacketBufferSize + 1, current_sequence ] (modulo 65536)

    void Advance( uint16_t sequence )
    {
        const uint16_t oldestSequence = sequence - PacketBufferSize + 1;

        for ( int i = 0; i < PacketBufferSize; ++i )
        {
            if ( valid[i] )
            {
                if ( protocol2::sequence_less_than( entries[i].sequence, oldestSequence ) )
                {
                    for ( int j = 0; j < entries[i].totalFragments; ++j )
                    {
                        if ( entries[i].fragments[j].data )
                        {
                            delete entries[i].fragments[j].data;
                            assert( numFragments > 0 );
                            numFragments--;
                        }
                    }
                }

                memset( &entries[i], 0, sizeof( PacketBufferEntry ) );

                valid[i] = false;
            }
        }

        currentSequence = sequence;
    }

    bool AddFragment( const uint8_t *data, int size, uint16_t sequence, uint8_t fragment_index, uint8_t total_fragments )
    {
        assert( data );
        assert( size > 0 );
        assert( size <= MaxFragmentSize );

        // is there too many buffered fragments for this client (globally, not per-entry) if so ignore this fragment

        // is the sequence number wildly out of range of the current sequence? if so ignore it.

        // does an entry already exist at the index?

        // if the entry exists, but has a different sequence number, ignore this fragment.

        // if the entry does not exist, add an entry for this sequence # and set total fragments

        // in the entry that at this point must exist, and have same sequence number...

        //    1. if the num fragments is different for this packet vs. the entry, ignore this fragment
        //    2. if the fragment index is out of range of [0,num_fragments-1] from the entry, ignore this fragment
        //    3. if a fragment already exists at the same fragment index, ignore this fragment
        //    4. if this is not the last fragment in the packet, if fragment size is not equal to MaxFragmentSize, ignore this fragment

        // increase received fragments for this entry

        // increase global fragment size

        // allocate fragment data and set size

        // copy across fragment data

        return true;
    }

    void ReadPacket( int & numPackets, PacketData packetData[] )
    {
        numPackets = 0;

        // is num fragments 0? early out return

        // calculate the index of the oldest packet given current_sequence

        // iterate i: 0 -> PacketBufferSize - 1, but look up entries ( i + index_oldest ) modulo PacketBufferSize

        // for each valid entry:

        //      1. calculate total size of packet (sum across all fragments)
        //      2. allocate a buffer inside the packet and set size
        //      3. copy across the packet data, iterating across all fragments in order
    }
};

static PacketBuffer packet_buffer;

enum TestPacketTypes
{
    PACKET_FRAGMENT = 0,                                // packet type 0 is reserved to indicate a packet fragment
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

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        serialize_int( stream, a, -10, 10 );
        serialize_int( stream, b, -20, 20 );
        serialize_int( stream, c, -30, 30 );
        return true;
    }

    bool operator == ( const TestPacketA & other ) const
    {
        return a == other.a && b == other.b && c == other.c;
    }

    bool operator != ( const TestPacketA & other ) const
    {
        return ! ( *this == other );
    }
};

static const int MaxItems = 4096;

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

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        serialize_int( stream, numItems, 0, MaxItems );
        for ( int i = 0; i < numItems; ++i )
            serialize_int( stream, items[i], -100, +100 );
        return true;
    }

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

    PROTOCOL2_SERIALIZE_OBJECT( stream )
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

int main()
{
    srand( time( NULL ) );

    TestPacketFactory packetFactory;

    for ( int i = 0; i < NumIterations; ++i )
    {
        const int packetType = 1 + rand() % ( TEST_PACKET_NUM_TYPES - 1 );          // packet type 0 indicate a packet fragment

        protocol2::Packet *writePacket = packetFactory.CreatePacket( packetType );

        assert( writePacket );
        assert( writePacket->GetType() == packetType );

        uint8_t buffer[MaxPacketSize];

        bool error = false;

        const int bytesWritten = protocol2::write_packet( writePacket, packetFactory, buffer, MaxPacketSize, ProtocolId );

        if ( bytesWritten > 0 )
        {
            printf( "wrote packet type %d (%d bytes)\n", writePacket->GetType(), bytesWritten );
        }
        else
        {
            printf( "write packet error\n" );
            error = true;
        }

        int readError;
        
        protocol2::Packet *readPacket = protocol2::read_packet( packetFactory, buffer, bytesWritten, ProtocolId, &readError );
        
        if ( readPacket )
        {
            printf( "read packet type %d (%d bytes)\n", readPacket->GetType(), bytesWritten );
            if ( !CheckPacketsAreIdentical( readPacket, writePacket ) )
            {
                printf( "read packet is not the same as written packet. something wrong with serialize function?\n" );
                error = true;
            }
        }
        else
        {
            printf( "read packet error: %s\n", protocol2::error_string( readError ) );
            error = true;
        }

        packetFactory.DestroyPacket( readPacket );
        packetFactory.DestroyPacket( writePacket );

        if ( error )
            return 1;
    }

    return 0;
}
