/*
    Protocol 2 by Glenn Fiedler <glenn.fiedler@gmail.com>
    This software is in the public domain. Where that dedication is not recognized, 
    you are granted a perpetual, irrevocable license to copy, distribute, and modify this file as you see fit.
*/

#define PROTOCOL2_IMPLEMENTATION

#include "protocol2.h"
#include <stdio.h>

#define check assert

void test_bitpacker()
{
    printf( "test_bitpacker\n" );

    const int BufferSize = 256;

    uint8_t buffer[256];

    protocol2::BitWriter writer( buffer, BufferSize );

    check( writer.GetData() == buffer );
    check( writer.GetTotalBytes() == BufferSize );
    check( writer.GetBitsWritten() == 0 );
    check( writer.GetBytesWritten() == 0 );
    check( writer.GetBitsAvailable() == BufferSize * 8 );

    writer.WriteBits( 0, 1 );
    writer.WriteBits( 1, 1 );
    writer.WriteBits( 10, 8 );
    writer.WriteBits( 255, 8 );
    writer.WriteBits( 1000, 10 );
    writer.WriteBits( 50000, 16 );
    writer.WriteBits( 9999999, 32 );
    writer.FlushBits();

    const int bitsWritten = 1 + 1 + 8 + 8 + 10 + 16 + 32;

    check( writer.GetBytesWritten() == 10 );
    check( writer.GetTotalBytes() == BufferSize );
    check( writer.GetBitsWritten() == bitsWritten );
    check( writer.GetBitsAvailable() == BufferSize * 8 - bitsWritten );

    protocol2::BitReader reader( buffer, BufferSize );

    check( reader.GetBitsRead() == 0 );
    check( reader.GetBitsRemaining() == BufferSize * 8 );

    uint32_t a = reader.ReadBits( 1 );
    uint32_t b = reader.ReadBits( 1 );
    uint32_t c = reader.ReadBits( 8 );
    uint32_t d = reader.ReadBits( 8 );
    uint32_t e = reader.ReadBits( 10 );
    uint32_t f = reader.ReadBits( 16 );
    uint32_t g = reader.ReadBits( 32 );

    check( a == 0 );
    check( b == 1 );
    check( c == 10 );
    check( d == 255 );
    check( e == 1000 );
    check( f == 50000 );
    check( g == 9999999 );

    check( reader.GetBitsRead() == bitsWritten );
    check( reader.GetBitsRemaining() == BufferSize * 8 - bitsWritten );
}

const int MaxItems = 16;

struct TestStreamObject : public protocol2::Object
{
    int a,b,c;
    uint32_t d : 8;
    uint32_t e : 8;
    uint32_t f : 8;
    bool g;
    int numItems;
    int items[MaxItems];

    TestStreamObject()
    {
        a = 0;
        b = 0;
        c = 0;
        d = 0;
        e = 0;
        f = 0;
        g = false;
        numItems = 0;
        memset( items, 0, sizeof(items) );
    }

    void Init()
    {
        a = 1;
        b = -2;
        c = 150;
        d = 55;
        e = 255;
        f = 127;
        g = true;
        numItems = MaxItems / 2;
        for ( int i = 0; i < numItems; ++i )
            items[i] = i + 10;        
    }

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        serialize_int( stream, a, 0, 10 );
        serialize_int( stream, b, -5, +5 );
        serialize_int( stream, c, -100, 10000 );

        serialize_bits( stream, d, 6 );
        serialize_bits( stream, e, 8 );
        serialize_bits( stream, f, 7 );

        serialize_bool( stream, g );

        serialize_int( stream, numItems, 0, MaxItems - 1 );
        for ( int i = 0; i < numItems; ++i )
            serialize_bits( stream, items[i], 8 );

        return true;
    }
};

// todo: should cover all cases of writing different integer types

// todo: should check for overflow, abort, invalid cases on read

// todo: should have test for crc32. randomly generate packets and pass into fuzz test

void test_stream_object()
{
    printf( "test_stream_object\n" );

    const int BufferSize = 256;

    uint8_t buffer[BufferSize];

    TestStreamObject writeObject;
    writeObject.Init();
    {
        protocol2::WriteStream writeStream( buffer, BufferSize );
        writeObject.SerializeWrite( writeStream );
        writeStream.Flush();
    }

    TestStreamObject readObject;
    {
        protocol2::ReadStream readStream( buffer, BufferSize );
        readObject.SerializeRead( readStream );
    }

    check( readObject.a == writeObject.a );
    check( readObject.b == writeObject.b );
    check( readObject.c == writeObject.c );
    check( readObject.d == writeObject.d );
    check( readObject.e == writeObject.e );
    check( readObject.f == writeObject.f );
    check( readObject.g == writeObject.g );
    check( readObject.numItems == writeObject.numItems );
    for ( int i = 0; i < readObject.numItems; ++i )
        check( readObject.items[i] == writeObject.items[i] );
}

struct TestContext
{
    int min;
    int max;
};

struct TestContextObject : public protocol2::Object
{
    int a,b;

    TestContextObject()
    {
        a = 0;
        b = 0;
    }

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        const TestContext & context = *(const TestContext*) stream.GetContext();
        serialize_int( stream, a, context.min, context.max );
        serialize_int( stream, b, context.min, context.max );
        return true;
    }
};

void test_stream_context()
{
    printf( "test_stream_context\n" );

    const int BufferSize = 256;

    uint8_t buffer[BufferSize];

    TestContext context;
    context.min = -10;
    context.max = +7;

    TestContextObject writeObject;
    writeObject.a = 2;
    writeObject.b = 7;
    {
        protocol2::WriteStream writeStream( buffer, BufferSize );

        check( writeStream.GetContext() == NULL );

        writeStream.SetContext( &context );

        check( writeStream.GetContext() == &context );

        writeObject.SerializeWrite( writeStream );

        writeStream.Flush();
    }

    TestContextObject readObject;
    {
        protocol2::ReadStream readStream( buffer, BufferSize );

        readStream.SetContext( &context );

        check( readStream.GetContext() == &context );

        readObject.SerializeRead( readStream );
    }

    check( readObject.a == writeObject.a );
    check( readObject.b == writeObject.b );
}

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

void test_packet_factory()
{
    printf( "test packet factory\n" );

    TestPacketFactory packetFactory;

    TestPacketA *a = (TestPacketA*) packetFactory.CreatePacket( TEST_PACKET_A );
    TestPacketB *b = (TestPacketB*) packetFactory.CreatePacket( TEST_PACKET_B );
    TestPacketC *c = (TestPacketC*) packetFactory.CreatePacket( TEST_PACKET_C );

    check( a );
    check( b );
    check( c );

    check( a->GetType() == TEST_PACKET_A );
    check( b->GetType() == TEST_PACKET_B );
    check( c->GetType() == TEST_PACKET_C );

    packetFactory.DestroyPacket( a );
    packetFactory.DestroyPacket( b );
    packetFactory.DestroyPacket( c );
}

// todo: test read and write packet w. multple packets

// todo: test read and wrte packet w. just one packet def

// todo: test the packets receved actually match what sent

int main()
{
    test_bitpacker();   
    test_stream_object();
    test_stream_context();
    test_packet_factory();
    return 0;
}
