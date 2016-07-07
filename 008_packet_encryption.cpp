/*
    Example source code for "Packet Encryption"

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

#ifdef _MSC_VER
#define SODIUM_STATIC
#endif // #ifdef _MSC_VER

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

using namespace protocol2;
using namespace network2;

const uint32_t ProtocolId = 0x12341651;

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

struct TestPacketA : public Packet
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

static const int MaxItems = 32;

struct TestPacketB : public Packet
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

struct TestPacketC : public Packet
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

struct TestPacketFactory : public PacketFactory
{
    TestPacketFactory() : PacketFactory( TEST_PACKET_NUM_TYPES ) {}

    Packet * Create( int type )
    {
        switch ( type )
        {
            case TEST_PACKET_A: return new TestPacketA();
            case TEST_PACKET_B: return new TestPacketB();
            case TEST_PACKET_C: return new TestPacketC();
        }
        return NULL;
    }

    void Destroy( Packet * packet )
    {
        delete packet;
    }
};

bool CheckPacketsAreIdentical( Packet * p1, Packet * p2 )
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

const int NonceBytes = 8;
const int KeyBytes = 32;
const int MacBytes = 16;
const int MaxPacketSize = 4096;
const int MaxPrefixBytes = 1 + 8;
const int EncryptionOverhead = MacBytes + MaxPrefixBytes;
const int MaxEncryptedPacketSize = 4096 + EncryptionOverhead;

void GenerateKey( uint8_t * key )
{
    assert( key );
    randombytes_buf( key, KeyBytes );
}

bool Encrypt( const uint8_t * message, int messageLength, 
              uint8_t * encryptedMessage, int & encryptedMessageLength, 
              const uint8_t * nonce, const uint8_t * key )
{
    uint8_t actual_nonce[crypto_secretbox_NONCEBYTES];
    memset( actual_nonce, 0, sizeof( actual_nonce ) );
    memcpy( actual_nonce, nonce, NonceBytes );

    if ( crypto_secretbox_easy( encryptedMessage, message, messageLength, actual_nonce, key ) != 0 )
        return false;

    encryptedMessageLength = messageLength + MacBytes;

    return true;
}

bool Decrypt( const uint8_t * encryptedMessage, int encryptedMessageLength, 
              uint8_t * decryptedMessage, int & decryptedMessageLength, 
              const uint8_t * nonce, const uint8_t * key )
{
    uint8_t actual_nonce[crypto_secretbox_NONCEBYTES];
    memset( actual_nonce, 0, sizeof( actual_nonce ) );
    memcpy( actual_nonce, nonce, NonceBytes );

    if ( crypto_secretbox_open_easy( decryptedMessage, encryptedMessage, encryptedMessageLength, actual_nonce, key ) != 0 )
        return false;

    decryptedMessageLength = encryptedMessageLength - MacBytes;

    return true;
}

static const int ENCRYPTED_PACKET_FLAG = 1 << 7;

const uint8_t * WriteAndEncryptPacket( PacketFactory & packetFactory, 
                                       Packet * packet, 
                                       uint64_t sequence, 
                                       uint8_t * packetBuffer, 
                                       uint8_t * scratchBuffer, 
                                       int & packetBytes, 
                                       const uint8_t * key )
{
    assert( packet );
    assert( packetBuffer );
    assert( scratchBuffer );
    assert( key );

    int prefixBytes;
    uint8_t prefix[16];
    CompressPacketSequence( sequence, prefix[0], prefixBytes, prefix+1 );
    prefix[0] |= ENCRYPTED_PACKET_FLAG;
    prefixBytes++;

    PacketInfo info;

    info.protocolId = ProtocolId;
    info.packetFactory = &packetFactory;
    info.rawFormat = 1;

    packetBytes = WritePacket( info, packet, scratchBuffer, MaxPacketSize );

    if ( packetBytes <= 0 )
    {
        printf( "error: failed to write packet\n" );
        return NULL;
    }

    int encryptedPacketSize;

    if ( !Encrypt( scratchBuffer,
                   packetBytes,
                   packetBuffer + prefixBytes,
                   encryptedPacketSize, 
                   (uint8_t*) &sequence, key ) )
    {
        printf( "error: failed to encrypt packet\n" );
        return NULL;
    }

    memcpy( packetBuffer, prefix, prefixBytes );

    packetBytes = prefixBytes + encryptedPacketSize;

    assert( packetBytes <= MaxEncryptedPacketSize );

    return packetBuffer;
}

Packet * DecryptAndReadPacket( PacketFactory & packetFactory, 
                               const uint8_t * packetData, 
                               uint8_t * scratchBuffer, 
                               uint64_t & sequence, 
                               int packetBytes, 
                               const uint8_t * key )
{
    assert( packetData );
    assert( scratchBuffer );
    assert( key );

    const uint8_t prefixByte = packetData[0];

    if ( ( prefixByte & ENCRYPTED_PACKET_FLAG ) == 0 )
    {
        printf( "error: packet is not encrypted\n" );
        return NULL;
    }

    const int sequenceBytes = GetPacketSequenceBytes( prefixByte );

    const int prefixBytes = 1 + sequenceBytes;

    if ( packetBytes <= prefixBytes + MacBytes )
    {
        printf( "error: packet is too small to possibly decrypt\n" );
        return NULL;
    }

    sequence = DecompressPacketSequence( prefixByte, packetData + 1 );

    int decryptedPacketBytes;

    if ( !Decrypt( packetData + prefixBytes, packetBytes - prefixBytes, scratchBuffer, decryptedPacketBytes, (uint8_t*)&sequence, key ) )
    {
        printf( "error: packet decrypt failed\n" );
        return NULL;
    }

    PacketInfo info;

    info.protocolId = ProtocolId;
    info.packetFactory = &packetFactory;
    info.rawFormat = 1;

    int readError;
    
    Packet * packet = ReadPacket( info, scratchBuffer, decryptedPacketBytes, NULL, &readError );

    if ( !packet )
    {
        printf( "error: failed to read packet\n" );
        return NULL;
    }
    
    return packet;
}

void PrintBytes( const char * label, const uint8_t * data, int data_bytes )
{
    printf( "%s: ", label );
    for ( int i = 0; i < data_bytes; ++i )
    {
        printf( "0x%02x,", (int) data[i] );
    }
    printf( " (%d bytes)\n", data_bytes );
}

int main()
{
    printf( "\npacket encryption\n\n" );

    assert( NonceBytes == crypto_aead_chacha20poly1305_NPUBBYTES );
    assert( KeyBytes == crypto_aead_chacha20poly1305_KEYBYTES );
    assert( MacBytes == crypto_secretbox_MACBYTES );

    if ( sodium_init() != 0 )
    {
        printf( "error: failed to initialize libsodium\n" );
        return 1;
    }

    uint8_t encryptionKey[KeyBytes];

    GenerateKey( encryptionKey );

    TestPacketFactory packetFactory;

    uint8_t packetBuffer[MaxEncryptedPacketSize];
    uint8_t scratchBuffer[MaxEncryptedPacketSize];

    const int NumPackets = 10;

    for ( int i = 0; i < NumPackets; ++i )
    {
        const int packetType = rand() % TEST_PACKET_NUM_TYPES;

        printf( "------------------------------------------------------\n" );

        printf( "created packet %d [type %d]\n", i, packetType );
    
        Packet * writePacket = packetFactory.CreatePacket( packetType );

        assert( writePacket );

        int writePacketBytes;
        const uint8_t * packetData = WriteAndEncryptPacket( packetFactory, writePacket, i, packetBuffer, scratchBuffer, writePacketBytes, encryptionKey );

        if ( !packetData )
            break;

        printf( "successfully encrypted packet\n" );

        PrintBytes( "encrypted packet", packetData, writePacketBytes );

        uint64_t sequence;
        Packet * readPacket = DecryptAndReadPacket( packetFactory, packetData, scratchBuffer, sequence, writePacketBytes, encryptionKey );
        if ( !readPacket )
        {
            packetFactory.DestroyPacket( readPacket );
            packetFactory.DestroyPacket( writePacket );
            break;
        }

        printf( "successfully decrypted packet\n" );

        if ( !CheckPacketsAreIdentical( readPacket, writePacket ) )
        {
            printf( "error: decrypted packet does not match packet before encryption!\n" );
            packetFactory.DestroyPacket( readPacket );
            packetFactory.DestroyPacket( writePacket );
            break;
        }

        packetFactory.DestroyPacket( readPacket );
        packetFactory.DestroyPacket( writePacket );
    }

    printf( "------------------------------------------------------\n" );

    printf( "\n" );

    return 0;
}
