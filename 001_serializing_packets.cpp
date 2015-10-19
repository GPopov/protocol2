/*
    Example source code for "Serializing Packets" in "Building a Game Network Protocol" series
    http://gafferongames.com/building-a-game-network-protocol/serializing-packets/

    This software is in the public domain. Where that dedication is not
    recognized, you are granted a perpetual, irrevocable license to copy,
    distribute, and modify this file as you see fit.
*/

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

int WritePacket( protocol2::Packet *packet, const TestPacketFactory & packetFactory, uint8_t *buffer, int bufferSize, uint32_t protocolId )
{
    assert( packet );
    assert( buffer );
    assert( bufferSize > 0 );
    assert( protocolId != 0 );

    protocol2::WriteStream stream( buffer, bufferSize );

    // write zero where the checksum will be

    stream.SerializeInteger( packet->GetType(), 0, packetFactory.GetNumTypes() );

    packet->SerializeWrite( stream );

    // todo: write magic at end

    // todo: calculate checksum (including shadow protocol id)

    // todo: write checksum (32bit)

    stream.Flush();

    assert( !stream.Aborted() );
    assert( !stream.IsOverflowed() );

    return stream.GetBytesProcessed();
}

protocol2::Packet* ReadPacket( TestPacketFactory & packetFactory, const uint8_t *buffer, int bufferSize, uint32_t protocolId )
{
    assert( buffer );
    assert( bufferSize > 0 );
    assert( protocolId != 0 );

    const int paddedSize = 4 * ( bufferSize / 4 ) + ( ( bufferSize % 4 ) ? 4 : 0 );

    assert( paddedSize >= bufferSize );

    // todo: read checksum, clear 4 bytes to zero, calculate checksum, discard packet if checksum doesn't match

    protocol2::ReadStream stream( buffer, paddedSize );

    int packetType;

    stream.SerializeInteger( packetType, 0, packetFactory.GetNumTypes() );

    // todo: need error checking here -- if outside range, throw away and don't create packet

    protocol2::Packet *packet = packetFactory.CreatePacket( packetType );

    if ( !packet )
        return NULL;

    packet->SerializeRead( stream );

    // todo: error handling. check if packet is overflowed, or if it is in error

    // todo: read magic, if magic fails, return NULL

    return packet;
}

int main()
{
    srand( time( NULL ) );

    TestPacketFactory packetFactory;

    for ( int i = 0; i < 10; ++i )
    {
        const int packetType = rand() % TEST_PACKET_NUM_TYPES;

        protocol2::Packet *packet = packetFactory.CreatePacket( packetType );

        assert( packet );
        assert( packet->GetType() == packetType );

        const uint32_t MaxPacketSize = 1024;
        const uint32_t ProtocolId = 0x12345678;

        uint8_t buffer[MaxPacketSize];

        const int bytes_written = WritePacket( packet, packetFactory, buffer, MaxPacketSize, ProtocolId );

        assert( bytes_written <= sizeof( buffer ) );

        packetFactory.DestroyPacket( packet );

        if ( bytes_written <= 0 )
        {
            printf( "failed to write packet\n" );
            exit(1);
        }

        protocol2::Packet *readPacket = ReadPacket( packetFactory, buffer, bytes_written, ProtocolId );

        if ( readPacket )
        {
            printf( "read packet type %d\n", readPacket->GetType() );

            packetFactory.DestroyPacket( readPacket );
        }
    }
    return 0;
}
