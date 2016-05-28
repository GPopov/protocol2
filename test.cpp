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

#include "yojimbo.h"
#include <stdio.h>
#include <stdlib.h>

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

    check( bytesWritten == 10 );

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

        serialize_check( stream, "test object serialize check" );

        serialize_int( stream, data.numItems, 0, MaxItems - 1 );
        for ( int i = 0; i < data.numItems; ++i )
            serialize_bits( stream, data.items[i], 8 );

        serialize_float( stream, data.float_value );

        serialize_double( stream, data.double_value );

        serialize_uint64( stream, data.uint64_value );

        serialize_bytes( stream, data.bytes, sizeof( data.bytes ) );

        serialize_string( stream, data.string, sizeof( data.string ) );

        serialize_check( stream, "end of test object" );

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
            data[i] = (uint8_t) i;
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

    protocol2::Packet* Create( int type )
    {
        switch ( type )
        {
            case TEST_PACKET_A: return new TestPacketA();
            case TEST_PACKET_B: return new TestPacketB();
            case TEST_PACKET_C: return new TestPacketC();
        }
        return NULL;
    }

    void Destroy( protocol2::Packet *packet )
    {
        delete packet;
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

#if 0

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

#endif // #if 0

void test_packet_sequence()
{
    printf( "test_packet_sequence\n" );

    {
        uint64_t sequence = 0x00001100223344;

        uint8_t prefix_byte;
        uint8_t sequence_bytes[8];
        int num_sequence_bytes;

        yojimbo::CompressPacketSequence( sequence, prefix_byte, num_sequence_bytes, sequence_bytes );

        check( prefix_byte == ( 1 | (1<<1) | (1<<3) ) );

        check( num_sequence_bytes == 4 );

        check( sequence_bytes[0] == 0x11 );
        check( sequence_bytes[1] == 0x22 );
        check( sequence_bytes[2] == 0x33 );
        check( sequence_bytes[3] == 0x44 );

        int decoded_num_sequence_bytes = yojimbo::GetPacketSequenceBytes( prefix_byte );

        check( decoded_num_sequence_bytes == num_sequence_bytes );

        uint64_t decoded_sequence = yojimbo::DecompressPacketSequence( prefix_byte, sequence_bytes );

        check( decoded_sequence == sequence );
    }

    for ( uint64_t sequence = 0; sequence < 100000000LL; sequence += 101 )
    {
        uint8_t prefix_byte;
        uint8_t sequence_bytes[8];
        int num_sequence_bytes;
        yojimbo::CompressPacketSequence( sequence, prefix_byte, num_sequence_bytes, sequence_bytes );
        uint64_t decoded_sequence = yojimbo::DecompressPacketSequence( prefix_byte, sequence_bytes );
        check( decoded_sequence == sequence );
    }
}

#if YOJIMBO_SECURE

void PrintBytes( const uint8_t * data, int data_bytes )
{
    for ( int i = 0; i < data_bytes; ++i )
    {
        printf( "%02x", (int) data[i] );
        if ( i != data_bytes - 1 )
            printf( "-" );
    }
    printf( " (%d bytes)", data_bytes );
}

#include <sodium.h>

void test_packet_encryption()
{
    printf( "test_packet_encryption\n" );

    using namespace yojimbo;

    uint8_t packet[1024];
  
    int packet_length = 1;
    memset( packet, 0, sizeof( packet ) );
    packet[0] = 1;  
  
    uint8_t key[KeyBytes];
    uint8_t nonce[NonceBytes];

    memset( key, 1, sizeof( key ) );
    memset( nonce, 1, sizeof( nonce ) );

    uint8_t encrypted_packet[2048];

    int encrypted_length;
    if ( !Encrypt( packet, packet_length, encrypted_packet, encrypted_length, nonce, key ) )
    {
        printf( "error: failed to encrypt\n" );
        exit(1);
    }

    const int expected_encrypted_length = 17;
    const uint8_t expected_encrypted_packet[] = { 0xfa, 0x6c, 0x91, 0xf7, 0xef, 0xdc, 0xed, 0x22, 0x09, 0x23, 0xd5, 0xbf, 0xa1, 0xe9, 0x17, 0x70, 0x14 };
    if ( encrypted_length != expected_encrypted_length || memcmp( expected_encrypted_packet, encrypted_packet, encrypted_length ) != 0 )
    {
        printf( "\npacket encryption failure!\n\n" );

        printf( " expected: " );
        PrintBytes( expected_encrypted_packet, expected_encrypted_length );
        printf( "\n" );

        printf( "      got: " );
        PrintBytes( encrypted_packet, encrypted_length );
        printf( "\n\n" );
    }

    uint8_t decrypted_packet[2048];
    int decrypted_length;
    if ( !Decrypt( encrypted_packet, encrypted_length, decrypted_packet, decrypted_length, nonce, key ) )
    {
        printf( "error: failed to decrypt\n" );
        exit(1);
    }

    if ( decrypted_length != packet_length || memcmp( packet, decrypted_packet, packet_length ) != 0 )
    {
        printf( "error: decrypted packet does not match original packet\n" );
        exit(1);
    }
}

#endif // #if YOJIMBO_SECURE

#include "network2.h"
#include "protocol2.h"
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <map>

using namespace std;
using namespace network2;
using namespace protocol2;
using namespace yojimbo;

const uint32_t ProtocolId = 0x12341651;

const int ClientPort = 40000;
const int ServerPort = 50000;

const int ConnectTokenBytes = 1024;
const int ChallengeTokenBytes = 256;
const int MaxServersPerConnectToken = 8;
const int ConnectTokenExpirySeconds = 10;

template <typename Stream> bool serialize_address_internal( Stream & stream, Address & address )
{
    char buffer[64];

    if ( Stream::IsWriting )
    {
        assert( address.IsValid() );
        address.ToString( buffer, sizeof( buffer ) );
    }

    serialize_string( stream, buffer, sizeof( buffer ) );

    if ( Stream::IsReading )
    {
        address = Address( buffer );
        if ( !address.IsValid() )
            return false;
    }

    return true;
}

#define serialize_address( stream, value )                          \
    do                                                              \
    {                                                               \
        if ( !serialize_address_internal( stream, value ) )         \
            return false;                                           \
    } while (0)

static uint8_t private_key[KeyBytes];

struct ConnectToken
{
    uint32_t protocolId;                                                // the protocol id this connect token corresponds to.
 
    uint64_t clientId;                                                  // the unique client id. max one connection per-client id, per-server.
 
    uint64_t expiryTimestamp;                                           // timestamp the connect token expires (eg. ~10 seconds after token creation)
 
    int numServerAddresses;                                             // the number of server addresses in the connect token whitelist.
 
    Address serverAddresses[MaxServersPerConnectToken];                 // connect token only allows connection to these server addresses.
 
    uint8_t clientToServerKey[KeyBytes];                                // the key for encrypted communication from client -> server.
 
    uint8_t serverToClientKey[KeyBytes];                                // the key for encrypted communication from server -> client.

    uint8_t random[KeyBytes];                                           // random data the client cannot possibly know.

    ConnectToken()
    {
        protocolId = 0;
        clientId = 0;
        expiryTimestamp = 0;
        numServerAddresses = 0;
        memset( clientToServerKey, 0, KeyBytes );
        memset( serverToClientKey, 0, KeyBytes );
        memset( random, 0, KeyBytes );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint32( stream, protocolId );

        serialize_uint64( stream, clientId );
        
        serialize_uint64( stream, expiryTimestamp );
        
        serialize_int( stream, numServerAddresses, 0, MaxServersPerConnectToken - 1 );
        
        for ( int i = 0; i < numServerAddresses; ++i )
            serialize_address( stream, serverAddresses[i] );

        serialize_bytes( stream, clientToServerKey, KeyBytes );

        serialize_bytes( stream, serverToClientKey, KeyBytes );

        serialize_bytes( stream, random, KeyBytes );

        return true;
    }
};

void GenerateConnectToken( ConnectToken & token, uint64_t clientId, int numServerAddresses, const Address * serverAddresses )
{
    uint64_t timestamp = (uint64_t) time( NULL );

    token.protocolId = ProtocolId;
    token.clientId = clientId;
    token.expiryTimestamp = timestamp + ConnectTokenExpirySeconds;
    
    assert( numServerAddresses > 0 );
    assert( numServerAddresses <= MaxServersPerConnectToken );
    token.numServerAddresses = numServerAddresses;
    for ( int i = 0; i < numServerAddresses; ++i )
        token.serverAddresses[i] = serverAddresses[i];

    GenerateKey( token.clientToServerKey );    

    GenerateKey( token.serverToClientKey );

    GenerateKey( token.random );
}

bool EncryptConnectToken( ConnectToken & token, uint8_t *encryptedMessage, const uint8_t *additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    uint8_t message[ConnectTokenBytes];
    memset( message, 0, ConnectTokenBytes );
    WriteStream stream( message, ConnectTokenBytes );
    if ( !token.Serialize( stream ) )
        return false;

    stream.Flush();
    
    if ( stream.GetError() )
        return false;

    uint64_t encryptedLength;

    if ( !Encrypt_AEAD( message, ConnectTokenBytes - AuthBytes, encryptedMessage, encryptedLength, additional, additionalLength, nonce, key ) )
        return false;

    assert( encryptedLength == ConnectTokenBytes );

    return true;
}

bool DecryptConnectToken( const uint8_t * encryptedMessage, ConnectToken & decryptedToken, const uint8_t * additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    const int encryptedMessageLength = ConnectTokenBytes;

    uint64_t decryptedMessageLength;
    uint8_t decryptedMessage[ConnectTokenBytes];

    if ( !Decrypt_AEAD( encryptedMessage, encryptedMessageLength, decryptedMessage, decryptedMessageLength, additional, additionalLength, nonce, key ) )
        return false;

    assert( decryptedMessageLength == ConnectTokenBytes - AuthBytes );

    ReadStream stream( decryptedMessage, ConnectTokenBytes - AuthBytes );
    if ( !decryptedToken.Serialize( stream ) )
        return false;

    if ( stream.GetError() )
        return false;

    return true;
}

struct ChallengeToken
{
    uint64_t clientId;                                                  // the unique client id. max one connection per-client id, per-server.

    Address clientAddress;                                              // client address corresponding to the initial connection request.

    Address serverAddress;                                              // client address corresponding to the initial connection request.

    uint8_t connectTokenMac[MacBytes];                                  // mac of the initial connect token this challenge corresponds to.
 
    uint8_t clientToServerKey[KeyBytes];                                // the key for encrypted communication from client -> server.
 
    uint8_t serverToClientKey[KeyBytes];                                // the key for encrypted communication from server -> client.

    uint8_t random[KeyBytes];                                           // random bytes the client cannot possibly know.

    ChallengeToken()
    {
        clientId = 0;
        memset( connectTokenMac, 0, MacBytes );
        memset( clientToServerKey, 0, KeyBytes );
        memset( serverToClientKey, 0, KeyBytes );
        memset( random, 0, KeyBytes );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, clientId );
        
        serialize_address( stream, clientAddress );

        serialize_address( stream, serverAddress );

        serialize_bytes( stream, connectTokenMac, MacBytes );

        serialize_bytes( stream, clientToServerKey, KeyBytes );

        serialize_bytes( stream, serverToClientKey, KeyBytes );

        serialize_bytes( stream, random, KeyBytes );

        return true;
    }
};

bool GenerateChallengeToken( const ConnectToken & connectToken, const Address & clientAddress, const Address & serverAddress, const uint8_t * connectTokenMac, ChallengeToken & challengeToken )
{
    if ( connectToken.clientId == 0 )
        return false;

    if ( !clientAddress.IsValid() )
        return false;

    challengeToken.clientId = connectToken.clientId;

    challengeToken.clientAddress = clientAddress;
    
    challengeToken.serverAddress = serverAddress;

    memcpy( challengeToken.connectTokenMac, connectTokenMac, MacBytes );

    memcpy( challengeToken.clientToServerKey, connectToken.clientToServerKey, KeyBytes );

    memcpy( challengeToken.serverToClientKey, connectToken.serverToClientKey, KeyBytes );

    GenerateKey( challengeToken.random );

    return true;
}

bool EncryptChallengeToken( ChallengeToken & token, uint8_t *encryptedMessage, const uint8_t *additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    uint8_t message[ChallengeTokenBytes];
    memset( message, 0, ChallengeTokenBytes );
    WriteStream stream( message, ChallengeTokenBytes );
    if ( !token.Serialize( stream ) )
        return false;

    stream.Flush();
    
    if ( stream.GetError() )
        return false;

    uint64_t encryptedLength;

    if ( !Encrypt_AEAD( message, ChallengeTokenBytes - AuthBytes, encryptedMessage, encryptedLength, additional, additionalLength, nonce, key ) )
        return false;

    assert( encryptedLength == ChallengeTokenBytes );

    return true;
}

bool DecryptChallengeToken( const uint8_t * encryptedMessage, ChallengeToken & decryptedToken, const uint8_t * additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    const int encryptedMessageLength = ChallengeTokenBytes;

    uint64_t decryptedMessageLength;
    uint8_t decryptedMessage[ChallengeTokenBytes];

    if ( !Decrypt_AEAD( encryptedMessage, encryptedMessageLength, decryptedMessage, decryptedMessageLength, additional, additionalLength, nonce, key ) )
        return false;

    assert( decryptedMessageLength == ChallengeTokenBytes - AuthBytes );

    ReadStream stream( decryptedMessage, ChallengeTokenBytes - AuthBytes );
    if ( !decryptedToken.Serialize( stream ) )
        return false;

    if ( stream.GetError() )
        return false;

    return true;
}

class Matcher
{
    uint64_t m_nonce;                                   // increments with each match request

public:

    Matcher()
    {
        m_nonce = 0;
    }

    bool RequestMatch( uint64_t clientId, uint8_t * tokenData, uint8_t * tokenNonce, uint8_t * clientToServerKey, uint8_t * serverToClientKey, int & numServerAddresses, Address * serverAddresses )
    {
        if ( clientId == 0 )
            return false;

        numServerAddresses = 1;
        serverAddresses[0] = Address( "::1", ServerPort );

        ConnectToken token;
        GenerateConnectToken( token, clientId, numServerAddresses, serverAddresses );

        memcpy( clientToServerKey, token.clientToServerKey, KeyBytes );
        memcpy( serverToClientKey, token.serverToClientKey, KeyBytes );

        if ( !EncryptConnectToken( token, tokenData, NULL, 0, (const uint8_t*) &m_nonce, private_key ) )
            return false;

        assert( NonceBytes == 8 );

        memcpy( tokenNonce, &m_nonce, NonceBytes );

        m_nonce++;

        return true;
    }
};

enum PacketTypes
{
    PACKET_CONNECTION_REQUEST,                      // client requests a connection.
    PACKET_CONNECTION_DENIED,                       // server denies client connection request.
    PACKET_CONNECTION_CHALLENGE,                    // server response to client connection request.
    PACKET_CONNECTION_RESPONSE,                     // client response to server connection challenge.
    PACKET_CONNECTION_HEARTBEAT,                    // heartbeat packet sent at some low rate (once per-second) to keep the connection alive.
    PACKET_CONNECTION_DISCONNECT,                   // courtesy packet to indicate that the other side has disconnected. better than a timeout.
    CLIENT_SERVER_NUM_PACKETS
};

struct ConnectionRequestPacket : public Packet
{
    uint8_t connectTokenData[ConnectTokenBytes];                        // encrypted connect token data generated by matchmaker
    uint8_t connectTokenNonce[NonceBytes];                              // nonce required to decrypt the connect token on the server

    ConnectionRequestPacket() : Packet( PACKET_CONNECTION_REQUEST )
    {
        memset( connectTokenData, 0, sizeof( connectTokenData ) );
        memset( connectTokenNonce, 0, sizeof( connectTokenNonce ) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bytes( stream, connectTokenData, sizeof( connectTokenData ) );
        serialize_bytes( stream, connectTokenNonce, sizeof( connectTokenNonce ) );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ConnectionDeniedPacket : public Packet
{
    ConnectionDeniedPacket() : Packet( PACKET_CONNECTION_DENIED ) {}

    template <typename Stream> bool Serialize( Stream & /*stream*/ ) { return true; }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ConnectionChallengePacket : public Packet
{
    uint8_t challengeTokenData[ChallengeTokenBytes];                      // encrypted challenge token data generated by matchmaker
    uint8_t challengeTokenNonce[NonceBytes];                              // nonce required to decrypt the challenge token on the server

    ConnectionChallengePacket() : Packet( PACKET_CONNECTION_CHALLENGE )
    {
        memset( challengeTokenData, 0, sizeof( challengeTokenData ) );
        memset( challengeTokenNonce, 0, sizeof( challengeTokenNonce ) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bytes( stream, challengeTokenData, sizeof( challengeTokenData ) );
        serialize_bytes( stream, challengeTokenNonce, sizeof( challengeTokenNonce ) );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ConnectionResponsePacket : public Packet
{
    uint8_t challengeTokenData[ChallengeTokenBytes];                      // encrypted challenge token data generated by matchmaker
    uint8_t challengeTokenNonce[NonceBytes];                              // nonce required to decrypt the challenge token on the server

    ConnectionResponsePacket() : Packet( PACKET_CONNECTION_RESPONSE )
    {
        memset( challengeTokenData, 0, sizeof( challengeTokenData ) );
        memset( challengeTokenNonce, 0, sizeof( challengeTokenNonce ) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_bytes( stream, challengeTokenData, sizeof( challengeTokenData ) );
        serialize_bytes( stream, challengeTokenNonce, sizeof( challengeTokenNonce ) );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ConnectionHeartBeatPacket : public Packet
{
    ConnectionHeartBeatPacket() : Packet( PACKET_CONNECTION_HEARTBEAT ) {}

    template <typename Stream> bool Serialize( Stream & /*stream*/ ) { return true; }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ConnectionDisconnectPacket : public Packet
{
    ConnectionDisconnectPacket() : Packet( PACKET_CONNECTION_DISCONNECT ) {}

    template <typename Stream> bool Serialize( Stream & /*stream*/ ) { return true; }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ClientServerPacketFactory : public PacketFactory
{
    ClientServerPacketFactory() : PacketFactory( CLIENT_SERVER_NUM_PACKETS ) {}

    Packet* Create( int type )
    {
        switch ( type )
        {
            case PACKET_CONNECTION_REQUEST:         return new ConnectionRequestPacket();
            case PACKET_CONNECTION_DENIED:          return new ConnectionDeniedPacket();
            case PACKET_CONNECTION_CHALLENGE:       return new ConnectionChallengePacket();
            case PACKET_CONNECTION_RESPONSE:        return new ConnectionResponsePacket();
            case PACKET_CONNECTION_HEARTBEAT:       return new ConnectionHeartBeatPacket();
            case PACKET_CONNECTION_DISCONNECT:      return new ConnectionDisconnectPacket();
            default:
                return NULL;
        }
    }

    void Destroy( Packet *packet )
    {
        delete packet;
    }
};

void test_packet_processor()
{
    printf( "test_packet_processor\n" );

    uint64_t clientId = 1;

    uint8_t connectTokenData[ConnectTokenBytes];
    uint8_t challengeTokenData[ChallengeTokenBytes];
    
    uint8_t connectTokenNonce[NonceBytes];
    uint8_t challengeTokenNonce[NonceBytes];

    uint8_t clientToServerKey[KeyBytes];
    uint8_t serverToClientKey[KeyBytes];

    int numServerAddresses;
    Address serverAddresses[MaxServersPerConnectToken];

    memset( connectTokenNonce, 0, NonceBytes );
    memset( challengeTokenNonce, 0, NonceBytes );

    printf( "\nrequesting match\n\n" );

    GenerateKey( private_key );

    Matcher matcher;

    if ( !matcher.RequestMatch( clientId, connectTokenData, connectTokenNonce, clientToServerKey, serverToClientKey, numServerAddresses, serverAddresses ) )
    {
        printf( "error: request match failed\n" );
        exit( 1 );
    }

    printf( "connect token: " );
    PrintBytes( connectTokenData, ConnectTokenBytes );
    printf( "\n" );

    ConnectToken connectToken;
    if ( !DecryptConnectToken( connectTokenData, connectToken, NULL, 0, connectTokenNonce, private_key ) )
    {
        printf( "error: failed to decrypt connect token\n" );
        exit( 1 );
    }

    assert( connectToken.clientId == 1 );
    assert( connectToken.numServerAddresses == 1 );
    assert( connectToken.serverAddresses[0] == Address( "::1", ServerPort ) );
    assert( memcmp( connectToken.clientToServerKey, clientToServerKey, KeyBytes ) == 0 );
    assert( memcmp( connectToken.serverToClientKey, serverToClientKey, KeyBytes ) == 0 );

    char serverAddressString[64];
    connectToken.serverAddresses[0].ToString( serverAddressString, sizeof( serverAddressString ) );
    printf( "\nsuccess: connect token is valid for client %" PRIx64 " connection to %s\n\n", connectToken.clientId, serverAddressString );

    Address clientAddress( "::1", ClientPort );

    ChallengeToken challengeToken;
    if ( !GenerateChallengeToken( connectToken, clientAddress, serverAddresses[0], connectTokenData, challengeToken ) )
    {
        printf( "error: failed to generate challenge token\n" );
        exit( 1 );
    }

    if ( !EncryptChallengeToken( challengeToken, challengeTokenData, NULL, 0, challengeTokenNonce, private_key ) )
    {
        printf( "error: failed to encrypt challenge token\n" );
        exit( 1 );
    }

    printf( "challenge token: " );
    PrintBytes( challengeTokenData, ChallengeTokenBytes );
    printf( "\n" );

    ChallengeToken decryptedChallengeToken;
    if ( !DecryptChallengeToken( challengeTokenData, decryptedChallengeToken, NULL, 0, challengeTokenNonce, private_key ) )
    {
        printf( "error: failed to decrypt challenge token\n" );
        exit( 1 );
    }

    assert( challengeToken.clientId == 1 );
    assert( challengeToken.clientAddress == clientAddress );
    assert( challengeToken.serverAddress == serverAddresses[0] );
    assert( memcmp( challengeToken.connectTokenMac, connectTokenData, MacBytes ) == 0 );
    assert( memcmp( challengeToken.clientToServerKey, clientToServerKey, KeyBytes ) == 0 );
    assert( memcmp( challengeToken.serverToClientKey, serverToClientKey, KeyBytes ) == 0 );

    char clientAddressString[64];
    challengeToken.clientAddress.ToString( clientAddressString, sizeof( clientAddressString ) );
    challengeToken.serverAddress.ToString( serverAddressString, sizeof( serverAddressString ) );
    printf( "\nsuccess: challenge token is valid for client %" PRIx64 " connection from %s to %s\n\n", challengeToken.clientId, clientAddressString, serverAddressString );

    ClientServerPacketFactory packetFactory;

    PacketProcessor packetProcessor( packetFactory, ProtocolId, 4096 );

    ConnectionChallengePacket * packet = (ConnectionChallengePacket*) packetFactory.CreatePacket( PACKET_CONNECTION_CHALLENGE );

    assert( packet );

    memcpy( packet->challengeTokenNonce, challengeTokenNonce, NonceBytes );

    if ( !EncryptChallengeToken( challengeToken, packet->challengeTokenData, NULL, 0, packet->challengeTokenNonce, private_key ) )
    {
        printf( "error: failed to encrypt challenge token\n" );
        exit( 1 );
    }

    printf( "server sent challenge packet to client\n\n" );

    int packetBytes;
    const uint8_t * packetData = packetProcessor.WritePacket( packet, 0, packetBytes, true, serverToClientKey );
    if ( !packetData )
    {
        printf( "error: failed to write connection challenge packet\n" );
        exit( 1 );
    }

    printf( "challenge packet: " );
    PrintBytes( packetData, packetBytes );
    printf( "\n\n" );

    packetFactory.DestroyPacket( packet );

    uint8_t encryptedPacketTypes[CLIENT_SERVER_NUM_PACKETS];
    uint8_t unencryptedPacketTypes[CLIENT_SERVER_NUM_PACKETS];

    memset( encryptedPacketTypes, 1, sizeof( encryptedPacketTypes ) );
    memset( unencryptedPacketTypes, 0, sizeof( unencryptedPacketTypes ) );

    uint64_t receivedSequence;
    Packet * receivedPacket = packetProcessor.ReadPacket( packetData, receivedSequence, packetBytes, serverToClientKey, encryptedPacketTypes, unencryptedPacketTypes );
    if ( !receivedPacket )
    {
        printf( "error: failed to read packet\n" );
        exit( 1 );
    }

    packetFactory.DestroyPacket( receivedPacket );
}

int main()
{
#if YOJIMBO_SECURE
    if ( !InitializeCrypto() )
    {
        printf( "error: failed to initialize crypto\n" );
        exit( 1 );
    }
#endif // #if YOJIMBO_SECURE

    test_bitpacker();   
    test_stream();
    test_packets();
    test_address_ipv4();
//    test_address_ipv6();
    test_packet_sequence();
    test_packet_encryption();
    test_packet_processor();

    return 0;
}
