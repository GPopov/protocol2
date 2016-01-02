/*
    Example source code for "Sending Large Blocks of Data Quickly and Reliably"

    http://gafferongames.com/building-a-game-network-protocol/sending-large-blocks-of-data-quickly-and-reliably

    This software is in the public domain. Where that dedication is not recognized, 
    you are granted a perpetual, irrevocable license to copy, distribute, and modify this file as you see fit.
*/

#define PROTOCOL2_IMPLEMENTATION

#include "protocol2.h"
#include <stdio.h>
#include <time.h>

const int MaxPacketSize = 1200;
const int ChunkSliceSize = 1024;
const int MaxSlicesPerChunk = 256;

const int NumIterations = 32;

const uint32_t ProtocolId = 0x11223344;

enum TestPacketTypes
{
    TEST_PACKET_CHUNK_SLICE,                    // this packet contains slice x out of y that makes up chunk n
    TEST_PACKET_CHUNK_ACK,                      // this packet acks a slice as being received by the other side

    TEST_PACKET_NUM_TYPES
};

struct TestPacketChunkSlice : public protocol2::Packet
{
    uint16_t chunkId;
    int sliceId;
    int numSlices;
    int sliceBytes;
    uint8_t data[ChunkSliceSize];

    TestPacketChunkSlice() : Packet( TEST_PACKET_CHUNK_SLICE )
    {
        chunkId = 0;
        sliceId = 0;
        numSlices = 0;
        sliceBytes = 0;
        memset( data, 0, sizeof( data ) );
    }

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        serialize_bits( stream, chunkId, 16 );
        serialize_int( stream, sliceId, 0, MaxSlicesPerChunk - 1 );
        serialize_int( stream, numSlices, 1, MaxSlicesPerChunk );
        if ( sliceId == numSlices - 1 )
        {
            serialize_int( stream, sliceBytes, 1, ChunkSliceSize );
        }
        else if ( Stream::IsReading )
        {
            sliceBytes = ChunkSliceSize;
        }
        serialize_bytes( stream, data, sliceBytes );
        return true;
    }
};

// IMPORTANT: because acks can be lost, must always when a slice is received for a chunk

struct TestPacketChunkAck : public protocol2::Packet
{
    uint16_t chunkId;
    int sliceId;
    int numSlices;
    bool acked[MaxSlicesPerChunk];

    TestPacketChunkAck() : Packet( TEST_PACKET_CHUNK_ACK )
    {
        chunkId = 0;
        sliceId = 0;
        numSlices = 0;
        memset( acked, 0, sizeof( acked ) );
    }

    PROTOCOL2_SERIALIZE_OBJECT( stream )
    {
        serialize_bits( stream, chunkId, 16 );
        serialize_int( stream, sliceId, 0, MaxSlicesPerChunk - 1 );
        serialize_int( stream, numSlices, 1, MaxSlicesPerChunk );
        for ( int i = 0; i < numSlices; ++i )
            serialize_bool( stream, acked[i] );
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
            case TEST_PACKET_CHUNK_SLICE:   return new TestPacketChunkSlice();
            case TEST_PACKET_CHUNK_ACK:     return new TestPacketChunkAck();
        }
        return NULL;
    }
};

// todo: need to split into sender and receiver structs

// todo: going to need a network simulator for this example (out of order, packet loss, latency...)

int main()
{
    srand( time( NULL ) );

    TestPacketFactory packetFactory;

    uint16_t sequence = 0;

    for ( int i = 0; i < NumIterations; ++i )
    {
        // ...
    }

    return 0;
}
