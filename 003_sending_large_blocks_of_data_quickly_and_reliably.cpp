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

enum PacketTypes
{
    SLICE_PACKET,                    // this packet contains slice x out of y that makes up chunk n
    ACK_PACKET,                      // this packet acks slices of chunk n that have been received
    NUM_PACKET_TYPES
};

struct SlicePacket : public protocol2::Packet
{
    uint16_t chunkId;
    int sliceId;
    int numSlices;
    int sliceBytes;
    uint8_t data[ChunkSliceSize];

    SlicePacket() : Packet( SLICE_PACKET )
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

// IMPORTANT: because acks can be lost, must always set a dirty flag when a slice is received for the current chunk id

struct AckPacket : public protocol2::Packet
{
    uint16_t chunkId;
    int sliceId;
    int numSlices;
    bool acked[MaxSlicesPerChunk];

    AckPacket() : Packet( ACK_PACKET )
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

struct PacketFactory : public protocol2::PacketFactory
{
    PacketFactory() : protocol2::PacketFactory( NUM_PACKET_TYPES ) {}

    protocol2::Packet* CreateInternal( int type )
    {
        switch ( type )
        {
            case SLICE_PACKET:   return new SlicePacket();
            case ACK_PACKET:     return new AckPacket();
        }
        return NULL;
    }
};

static PacketFactory packetFactory;

class ChunkSender
{
    bool sending;                                               // true if we are currently sending a chunk. can only send one chunk at a time
    uint16_t chunkId;                                           // the chunk id. starts at 0 and increases as each chunk is successfully sent and acked.
    int chunkSize;                                              // the size of the chunk that is being sent in bytes
    int sliceId;                                                // the slice of the chunk that is to be sent next. iteration is from left -> right wrapping past the last slice back to 0.
    int numSlices;                                              // the number of slices in the current chunk being sent
    bool acked[MaxSlicesPerChunk];                              // acked flag for each slice of the chunk. chunk send completes when all slices are acked. acked slices are skipped when iterating for next slice to send.
    double timeLastSent[MaxSlicesPerChunk];                     // time the slice of the chunk was last sent. avoids redundant behavior
    uint8_t chunkData[MaxSlicesPerChunk*ChunkSliceSize];        // chunk data in 

    ChunkSender()
    {
        memset( this, 0, sizeof( ChunkSender ) );
    }

    void SendChunk()
    {
        assert( SendCompleted() );

        // ...
    }

    bool SendCompleted()
    {
        return !sending;
    }

    // todo: make sure the algorithm iterates from left -> right always in passes
    // vs. continuously iterating across the same slices left -> right always

    SlicePacket* SendSlicePacket( double t )
    {
        // todo: might not be time to send another slice yet, in which case return NULL?

        // ...

        return NULL;
    }

    void ProcessAckPacket( AckPacket *packet )
    {
        // ...
    }
};

class ChunkReceiver
{
    uint16_t chunkId;

    AckPacket* SendAckPacket()
    {
        // ...

        return NULL;
    }

    void ProcessSlicePacket( SlicePacket *packet )
    {
        // ...
    }
};

// todo: need to split into sender and receiver structs

// todo: going to need a network simulator for this example (out of order, packet loss, latency...)

int main()
{
    srand( time( NULL ) );

    for ( int i = 0; i < NumIterations; ++i )
    {
        // ...
    }

    return 0;
}
