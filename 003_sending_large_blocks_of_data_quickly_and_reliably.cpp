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
const int MaxChunkSize = ChunkSliceSize * MaxSlicesPerChunk;

const float SliceMinimumResendTime = 0.1f;

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

    PROTOCOL2_SERIALIZE_FUNCTION( stream )
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

    PROTOCOL2_SERIALIZE_FUNCTION( stream )
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
    int numSlices;                                              // the number of slices in the current chunk being sent
    int currentSliceId;                                         // the current slice id to be considered next time we send a slice packet. iteration starts here.
    bool acked[MaxSlicesPerChunk];                              // acked flag for each slice of the chunk. chunk send completes when all slices are acked. acked slices are skipped when iterating for next slice to send.
    double timeLastSent[MaxSlicesPerChunk];                     // time the slice of the chunk was last sent. avoids redundant behavior
    uint8_t chunkData[MaxChunkSize];                            // chunk data being sent.

public:

    ChunkSender()
    {
        memset( this, 0, sizeof( ChunkSender ) );
    }

    void SendChunk( const uint8_t *data, int size )
    {
        assert( data );
        assert( size > 0 );
        assert( size <= MaxChunkSize );
        assert( SendCompleted() );
        sending = true;
        chunkSize = size;
        currentSliceId = 0;
        numSlices = ( size + ChunkSliceSize - 1 ) / ChunkSliceSize;
        assert( numSlices > 0 );
        assert( numSlices < MaxSlicesPerChunk );
        memset( acked, 0, sizeof( acked ) );
        memset( timeLastSent, 0, sizeof( timeLastSent ) );
        memcpy( chunkData, data, size );
    }

    bool SendCompleted()
    {
        return !sending;
    }

    SlicePacket* SendSlicePacket( double t )
    {
        if ( !sending ) 
            return NULL;

        for ( int i = 0; i < numSlices; ++i )
        {
            const int sliceId = ( currentSliceId + i ) % numSlices;

            if ( acked[sliceId] )
            {
                currentSliceId = ( sliceId + 1 ) % numSlices;
                continue;
            }

            if ( timeLastSent[sliceId] + SliceMinimumResendTime >= t )
            {
                currentSliceId = ( sliceId + 1 ) % numSlices;
                SlicePacket *packet = (SlicePacket*) packetFactory.CreatePacket( SLICE_PACKET );
                packet->chunkId = chunkId;
                packet->sliceId = sliceId;
                packet->numSlices = numSlices;
                packet->sliceBytes = ( sliceId == numSlices - 1 ) ? ( chunkSize % ChunkSliceSize ) : ChunkSliceSize;
                memcpy( packet->data, chunkData + sliceId * ChunkSliceSize, packet->sliceBytes );
                printf( "sent slice %d of chunk %d (%d bytes)\n", sliceId, chunkId, packet->sliceBytes );
                return packet;
            }
        }

        return NULL;
    }

    bool ProcessAckPacket( AckPacket *packet )
    {
        assert( packet );

        if ( packet->chunkId != chunkId )
            return false;



        return true;
    }
};

class ChunkReceiver
{
    uint16_t chunkId;

public:

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

    ChunkSender sender;
    ChunkReceiver receiver;

    // ...

    return 0;
}
