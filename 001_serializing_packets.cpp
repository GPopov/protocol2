/*
    Example source code for "Serializing Packets"
    http://gafferongames.com/building-a-game-network-protocol/serializing-packets/
    This software is in the public domain. Where that dedication is not recognized, 
    you are granted a perpetual, irrevocable license to copy, distribute, and modify this file as you see fit.
*/

#include "protocol2.h"
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

struct TestPacketA : public protocol2::Packet
{
    int a,b,c;

    TestPacketA() : Packet( TEST_PACKET_A )
    {
        a = 1;
        b = 2;
        c = 3;        
    }

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        serialize_int( stream, a, -10, 10 );
        serialize_int( stream, b, -20, 20 );
        serialize_int( stream, c, -30, 30 );
        return true;
    }
};

struct TestPacketB : public protocol2::Packet
{
    int x, y;

    TestPacketB() : Packet( TEST_PACKET_B )
    {
        x = 0;
        y = 1;
    }

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        serialize_int( stream, x, -5, +5 );
        serialize_int( stream, y, -5, +5 );
        return true;
    }
};

struct TestPacketC : public protocol2::Packet
{
    uint8_t data[16];

    TestPacketC() : Packet( TEST_PACKET_C )
    {
        for ( int i = 0; i < sizeof( data ); ++i )
            data[i] = i;
    }

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        for ( int i = 0; i < sizeof( data ); ++i )
            serialize_int( stream, data[i], 0, 255 );
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

    int num_packets_written = 0;
    int num_packets_read = 0;

    const int num_iterations = 10;

    for ( int i = 0; i < num_iterations; ++i )
    {
        const int packetType = rand() % TEST_PACKET_NUM_TYPES;

        protocol2::Packet *packet = packetFactory.CreatePacket( packetType );

        assert( packet );
        assert( packet->GetType() == packetType );

        const uint32_t MaxPacketSize = 1024;
        const uint32_t ProtocolId = 0x12345678;

        uint8_t buffer[MaxPacketSize];

        const int bytes_written = protocol2::write_packet( packet, packetFactory, buffer, MaxPacketSize, ProtocolId );

        assert( bytes_written <= sizeof( buffer ) );

        packetFactory.DestroyPacket( packet );

        if ( bytes_written > 0 )
        {
            num_packets_written++;
        }
        else
        {
            printf( "failed to write packet\n" );
        }

        printf( "bytes written = %d\n", bytes_written );

        protocol2::Packet *readPacket = protocol2::read_packet( packetFactory, buffer, bytes_written, ProtocolId );

        if ( readPacket )
        {
            printf( "read packet type %d\n", readPacket->GetType() );
            packetFactory.DestroyPacket( readPacket );
            num_packets_read++;
        }
        else
        {
            printf( "failed to read packet type %d\n", packetType );        // todo: print out error reason
        }
    }

    printf( "num_iterations = %d\n", num_iterations );
    printf( "num_packets_read = %d\n", num_packets_read );
    printf( "num_packets_written = %d\n", num_packets_written );

    if ( num_packets_written == num_iterations && num_packets_read == num_iterations )
    {
        printf( "success.\n" );
        return 0;
    }
    else
    {
        printf( "failure\n" );
        return 1;
    }
}
