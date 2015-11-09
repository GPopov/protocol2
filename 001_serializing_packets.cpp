/*
    Example source code for "Serializing Packets"
    http://gafferongames.com/building-a-game-network-protocol/serializing-packets/
    This software is in the public domain. Where that dedication is not recognized, 
    you are granted a perpetual, irrevocable license to copy, distribute, and modify this file as you see fit.
*/

#define PROTOCOL2_IMPLEMENTATION

#include "protocol2.h"
#include <stdio.h>
#include <time.h>
#include <list>

enum TestPacketTypes
{
    TEST_PACKET_A,
    TEST_PACKET_B,
    TEST_PACKET_C,
    TEST_PACKET_NUM_TYPES
};

inline int random_int( int min, int max )
{
    assert( max > min );
    int result = min + rand() % ( max - min + 1 );
    assert( result >= min );
    assert( result <= max );
    return result;
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
};

static const int MaxItems = 32;

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
};

struct Vector
{
    float x,y,z;
};

inline float random_float( float min, float max )
{
    const int res = 10000000;
    double scale = ( rand() % res ) / double( res - 1 );
    return (float) ( min + (double) ( max - min ) * scale );
}

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

int main()
{
    srand( time( NULL ) );

    TestPacketFactory packetFactory;

    int numPacketsWritten = 0;
    int numPacketsRead = 0;

    const int NumIterations = 10;

    for ( int i = 0; i < NumIterations; ++i )
    {
        const int packetType = rand() % TEST_PACKET_NUM_TYPES;

        protocol2::Packet *packet = packetFactory.CreatePacket( packetType );

        assert( packet );
        assert( packet->GetType() == packetType );

        const uint32_t MaxPacketSize = 1024;
        const uint32_t ProtocolId = 0x12345678;

        uint8_t buffer[MaxPacketSize];

        const int bytesWritten = protocol2::write_packet( packet, packetFactory, buffer, MaxPacketSize, ProtocolId );

        packetFactory.DestroyPacket( packet );

        if ( bytesWritten > 0 )
        {
            printf( "wrote packet type %d (%d bytes)\n", packet->GetType(), bytesWritten );
            numPacketsWritten++;
        }
        else
        {
            printf( "failed to write packet\n" );
        }

        int readError;
        protocol2::Packet *readPacket = protocol2::read_packet( packetFactory, buffer, bytesWritten, ProtocolId, &readError );

        if ( readPacket )
        {
            printf( "read packet type %d (%d bytes)\n", readPacket->GetType(), bytesWritten );
            packetFactory.DestroyPacket( readPacket );
            numPacketsRead++;
        }
        else
        {
            printf( "failed to read packet: %d\n", readError );
        }
    }

    if ( numPacketsWritten == NumIterations && numPacketsRead == NumIterations )
    {
        printf( "success.\n" );
        return 0;
    }
    else
    {
        printf( "failure.\n" );
        return 1;
    }
}
