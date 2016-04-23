/*
    Functional Tests for Protocol2 Library and Network2 Library.

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

#define NETWORK2_IMPLEMENTATION
#define PROTOCOL2_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"
#include <stdio.h>

static void CheckHandler( const char * condition, 
                          const char * function,
                          const char * file,
                          int line )
{
    printf( "check failed: ( %s ), function %s, file %s, line %d\n", condition, function, file, line );
#ifndef NDEBUG
    #if defined( __GNUC__ )
        __builtin_trap();
    #elif defined( _MSC_VER )
        __debugbreak();
    #endif
#endif
    exit( 1 );
}

#define check( condition )                                                     \
do                                                                             \
{                                                                              \
    if ( !(condition) )                                                        \
    {                                                                          \
        CheckHandler( #condition, __FUNCTION__, __FILE__, __LINE__ );          \
    }                                                                          \
} while(0)

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

    const int bytesWritten = writer.GetBytesWritten();

    assert( bytesWritten == 10 );

    memset( buffer + bytesWritten, 0, BufferSize - bytesWritten );

    protocol2::BitReader reader( buffer, bytesWritten );

    check( reader.GetBitsRead() == 0 );
    check( reader.GetBitsRemaining() == bytesWritten * 8 );

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
    check( reader.GetBitsRemaining() == bytesWritten * 8 - bitsWritten );
}

const int MaxItems = 11;

struct TestData
{
    int a,b,c;
    uint32_t d : 8;
    uint32_t e : 8;
    uint32_t f : 8;
    bool g;
    int numItems;
    int items[MaxItems];
    float float_value;
    double double_value;
    uint64_t uint64_value;
    uint8_t bytes[17];
    char string[64];
};

struct TestContext
{
    int min;
    int max;
};

struct TestObject : public protocol2::Object
{
    TestData data;

    TestObject()
    {
        memset( &data, 0, sizeof( data ) );
    }

    void Init()
    {
        data.a = 1;
        data.b = -2;
        data.c = 150;
        data.d = 55;
        data.e = 255;
        data.f = 127;
        data.g = true;

        data.numItems = MaxItems / 2;
        for ( int i = 0; i < data.numItems; ++i )
            data.items[i] = i + 10;     

        data.float_value = 3.1415926f;
        data.double_value = 1 / 3.0;   
        data.uint64_value = 0x1234567898765432L;

        for ( int i = 0; i < (int) sizeof( data.bytes ); ++i )
            data.bytes[i] = rand() % 255;

        strcpy( data.string, "hello world!" );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        const TestContext & context = *(const TestContext*) stream.GetContext();

        serialize_int( stream, data.a, context.min, context.max );
        serialize_int( stream, data.b, context.min, context.max );

        serialize_int( stream, data.c, -100, 10000 );

        serialize_bits( stream, data.d, 6 );
        serialize_bits( stream, data.e, 8 );
        serialize_bits( stream, data.f, 7 );

        serialize_align( stream );

        serialize_bool( stream, data.g );

        serialize_check( stream, 0x55225500 );

        serialize_int( stream, data.numItems, 0, MaxItems - 1 );
        for ( int i = 0; i < data.numItems; ++i )
            serialize_bits( stream, data.items[i], 8 );

        serialize_float( stream, data.float_value );

        serialize_double( stream, data.double_value );

        serialize_uint64( stream, data.uint64_value );

        serialize_bytes( stream, data.bytes, sizeof( data.bytes ) );

        serialize_string( stream, data.string, sizeof( data.string ) );

        serialize_check( stream, 0x12341111 );

        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();

    bool operator == ( const TestObject & other ) const
    {
        return memcmp( &data, &other.data, sizeof( TestData ) ) == 0;
    }

    bool operator != ( const TestObject & other ) const
    {
        return ! ( *this == other );
    }
};

void test_stream()
{
    printf( "test_stream\n" );

    const int BufferSize = 1024;

    uint8_t buffer[BufferSize];

    TestContext context;
    context.min = -10;
    context.max = +10;

    protocol2::WriteStream writeStream( buffer, BufferSize );

    TestObject writeObject;
    writeObject.Init();
    writeStream.SetContext( &context );
    writeObject.SerializeWrite( writeStream );
    writeStream.Flush();

    const int bytesWritten = writeStream.GetBytesProcessed();

    memset( buffer + bytesWritten, 0, BufferSize - bytesWritten );

    TestObject readObject;
    protocol2::ReadStream readStream( buffer, bytesWritten );
    readStream.SetContext( &context );
    readObject.SerializeRead( readStream );

    check( readObject == writeObject );
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

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_int( stream, a, -10, 10 );
        serialize_int( stream, b, -20, 20 );
        serialize_int( stream, c, -30, 30 );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct TestPacketB : public protocol2::Packet
{
    int x,y;

    TestPacketB() : Packet( TEST_PACKET_B )
    {
        x = 0;
        y = 1;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_int( stream, x, -5, +5 );
        serialize_int( stream, y, -5, +5 );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct TestPacketC : public protocol2::Packet
{
    uint8_t data[16];

    TestPacketC() : Packet( TEST_PACKET_C )
    {
        for ( int i = 0; i < (int) sizeof( data ); ++i )
            data[i] = i;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        for ( int i = 0; i < (int) sizeof( data ); ++i )
            serialize_int( stream, data[i], 0, 255 );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
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

void test_packets()
{
    printf( "test packets\n" );

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

void test_address_ipv4()
{
    printf( "test_address_ipv4\n" );

    char buffer[256];

    {
        network2::Address address( 127, 0, 0, 1 );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV4 );
        check( address.GetPort() == 0 );
        check( address.GetAddress4() == 0x100007f );
        check( strcmp( address.ToString( buffer, 256 ), "127.0.0.1" ) == 0 );
    }

    {
        network2::Address address( 127, 0, 0, 1, 1000 );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV4 );
        check( address.GetPort() == 1000 );
        check( address.GetAddress4() == 0x100007f );
        check( strcmp( address.ToString( buffer, 256 ), "127.0.0.1:1000" ) == 0 );
    }

    {
        network2::Address address( "127.0.0.1" );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV4 );
        check( address.GetPort() == 0 );
        check( address.GetAddress4() == 0x100007f );
        check( strcmp( address.ToString( buffer, 256 ), "127.0.0.1" ) == 0 );
    }

    {
        network2::Address address( "127.0.0.1:65535" );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV4 );
        check( address.GetPort() == 65535 );
        check( address.GetAddress4() == 0x100007f );
        check( strcmp( address.ToString( buffer, 256 ), "127.0.0.1:65535" ) == 0 );
    }

    {
        network2::Address address( "10.24.168.192:3000" );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV4 );
        check( address.GetPort() == 3000 );
        check( address.GetAddress4() == 0xc0a8180a );
        check( strcmp( address.ToString( buffer, 256 ), "10.24.168.192:3000" ) == 0 );
    }

    {
        network2::Address address( "255.255.255.255:65535" );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV4 );
        check( address.GetPort() == 65535 );
        check( address.GetAddress4() == 0xffffffff );
        check( strcmp( address.ToString( buffer, 256 ), "255.255.255.255:65535" ) == 0 );
    }
}

void test_address_ipv6()
{
    printf( "test_address_ipv6\n" );

    char buffer[256];

    // without port numbers

    {
        const uint16_t address6[] = { 0xFE80, 0x0000, 0x0000, 0x0000, 0x0202, 0xB3FF, 0xFE1E, 0x8329 };

        network2::Address address( address6[0], address6[1], address6[2], address6[2],
                                   address6[4], address6[5], address6[6], address6[7] );

        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 0 );

        for ( int i = 0; i < 8; ++i )
            check( htons( address6[i] ) == address.GetAddress6()[i] );

        check( strcmp( address.ToString( buffer, 256 ), "fe80::202:b3ff:fe1e:8329" ) == 0 );
    }

    {
        const uint16_t address6[] = { 0xFE80, 0x0000, 0x0000, 0x0000, 0x0202, 0xB3FF, 0xFE1E, 0x8329 };

        network2::Address address( address6 );

        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 0 );

        for ( int i = 0; i < 8; ++i )
            check( htons( address6[i] ) == address.GetAddress6()[i] );

        check( strcmp( address.ToString( buffer, 256 ), "fe80::202:b3ff:fe1e:8329" ) == 0 );
    }

    {
        const uint16_t address6[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001 };

        network2::Address address( address6 );

        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 0 );

        for ( int i = 0; i < 8; ++i )
            check( htons( address6[i] ) == address.GetAddress6()[i] );

        check( strcmp( address.ToString( buffer, 256 ), "::1" ) == 0 );
    }

    // same addresses but with port numbers

    {
        const uint16_t address6[] = { 0xFE80, 0x0000, 0x0000, 0x0000, 0x0202, 0xB3FF, 0xFE1E, 0x8329 };

        network2::Address address( address6[0], address6[1], address6[2], address6[2],
                                  address6[4], address6[5], address6[6], address6[7], 65535 );

        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 65535 );

        for ( int i = 0; i < 8; ++i )
            check( htons( address6[i] ) == address.GetAddress6()[i] );

        check( strcmp( address.ToString( buffer, 256 ), "[fe80::202:b3ff:fe1e:8329]:65535" ) == 0 );
    }

    {
        const uint16_t address6[] = { 0xFE80, 0x0000, 0x0000, 0x0000, 0x0202, 0xB3FF, 0xFE1E, 0x8329 };

        network2::Address address( address6, 65535 );

        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 65535 );

        for ( int i = 0; i < 8; ++i )
            check( htons( address6[i] ) == address.GetAddress6()[i] );

        check( strcmp( address.ToString( buffer, 256 ), "[fe80::202:b3ff:fe1e:8329]:65535" ) == 0 );
    }

    {
        const uint16_t address6[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001 };

        network2::Address address( address6, 65535 );

        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 65535 );

        for ( int i = 0; i < 8; ++i )
            check( htons( address6[i] ) == address.GetAddress6()[i] );

        check( strcmp( address.ToString( buffer, 256 ), "[::1]:65535" ) == 0 );
    }

    // parse addresses from strings (no ports)

    {
        network2::Address address( "fe80::202:b3ff:fe1e:8329" );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 0 );
        check( strcmp( address.ToString( buffer, 256 ), "fe80::202:b3ff:fe1e:8329" ) == 0 );
    }

    {
        network2::Address address( "::1" );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 0 );
        check( strcmp( address.ToString( buffer, 256 ), "::1" ) == 0 );
    }

    // parse addresses from strings (with ports)

    {
        network2::Address address( "[fe80::202:b3ff:fe1e:8329]:65535" );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 65535 );
        check( strcmp( address.ToString( buffer, 256 ), "[fe80::202:b3ff:fe1e:8329]:65535" ) == 0 );
    }

    {
        network2::Address address( "[::1]:65535" );
        check( address.IsValid() );
        check( address.GetType() == network2::ADDRESS_IPV6 );
        check( address.GetPort() == 65535 );
        check( strcmp( address.ToString( buffer, 256 ), "[::1]:65535" ) == 0 );
    }
}

int main()
{
    test_bitpacker();   
    test_stream();
    test_packets();
    test_address_ipv4();
    test_address_ipv6();
    return 0;
}
