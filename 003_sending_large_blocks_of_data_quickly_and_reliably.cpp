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
const float MinimumTimeBetweenAcks = 0.1f;

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
    int numAckedSlices;                                         // number of slices acked by the receiver. when num slices acked = num slices, the send is completed.
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
        numAckedSlices = 0;
        numSlices = ( size + ChunkSliceSize - 1 ) / ChunkSliceSize;
        assert( numSlices > 0 );
        assert( numSlices < MaxSlicesPerChunk );
        memset( acked, 0, sizeof( acked ) );
        memset( timeLastSent, 0, sizeof( timeLastSent ) );
        memcpy( chunkData, data, size );
        printf( "sending chunk %d of size %d bytes in %d slices\n", chunkId, chunkSize, numSlices );
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

        if ( !sending )
            return false;

        if ( packet->chunkId != chunkId )
            return false;

        if ( packet->numSlices != numSlices )
            return false;

        for ( int i = 0; i < numSlices; ++i )
        {
            if ( acked[i] == false && packet->acked[i] )
            {
                printf( "acked slice %d of chunk %d\n", i, chunkId );
                acked[i] = true;
                numAckedSlices++;
                assert( numAckedSlices >= 0 );
                assert( numAckedSlices <= numSlices );
                if ( numAckedSlices == numSlices )
                {
                    printf( "all slices of chunk %d acked, send completed\n", chunkId );
                    sending = false;
                    chunkId++;
                }
            }
        }

        return true;
    }
};

class ChunkReceiver
{
    bool receiving;                                             // true if we are currently receiving a chunk.
    bool readyToRead;                                           // true if a chunk has been received and is ready for the caller to read.
    bool forceAckPreviousChunk;                                 // if this flag is set then we need to send a complete ack for the previous chunk id (sender has not yet received an ack with all slices received)
    uint16_t chunkId;                                           // id of the chunk that is currently being received, or
    int chunkSize;                                              // the size of the chunk that has been received. only known once the last slice has been received!
    int numSlices;                                              // the number of slices in the current chunk being sent
    int numReceivedSlices;                                      // number of slices received for the current chunk. when num slices receive = num slices, the receive is complete.
    double timeLastAckSent;                                     // time last ack was sent. used to rate limit acks to some maximum number of acks per-second. 
    bool received[MaxSlicesPerChunk];                           // received flag for each slice of the chunk. chunk receive completes when all slices are received.
    uint8_t chunkData[MaxChunkSize];                            // chunk data being received.

public:

    ChunkReceiver()
    {
        memset( this, 0, sizeof( ChunkReceiver ) );
    }

    bool ProcessSlicePacket( SlicePacket *packet )
    {
        assert( packet );

        // caller has read the chunk before we can receive the next one
        if ( readyToRead )
            return false;

        if ( !receiving && packet->chunkId == chunkId - 1 )
        {
            // this happens because the sender has not yet seen an ack for all slices (packet loss) even though the sender has received them all.
            // if we don't force an ack, the sender will be stuck with unacked slices from the previous chunk and won't ever complete the send.
            forceAckPreviousChunk = true;
        }

        if ( !receiving && packet->chunkId == chunkId )
        {
            printf( "started receiving chunk %d\n", chunkId );

            assert( !readyToRead );
            
            receiving = true;
            forceAckPreviousChunk = false;
            numReceivedSlices = 0;
            chunkSize = 0;
            
            numSlices = packet->numSlices;
            assert( numSlices > 0 );
            assert( numSlices <= MaxSlicesPerChunk );

            memset( received, 0, sizeof( received ) );
        }

        if ( packet->chunkId != chunkId )
            return false;

        if ( packet->numSlices != numSlices )
            return false;

        assert( packet->sliceId >= 0 );
        assert( packet->sliceId <= numSlices );

        if ( !received[packet->sliceId] )
        {
            printf( "received slice %d of chunk %d\n", packet->sliceId, chunkId );

            received[packet->sliceId] = true;

            numReceivedSlices++;

            assert( numReceivedSlices > 0 );
            assert( numReceivedSlices <= numSlices );

            if ( packet->sliceId == numSlices - 1 )
                chunkSize = ( numSlices - 1 ) * packet->sliceBytes;

            if ( numReceivedSlices == numSlices )
            {
                receiving = false;
                readyToRead = true;
                chunkId++;
            }
        }

        return true;
    }

    AckPacket* SendAckPacket( double t )
    {
        if ( timeLastAckSent + MinimumTimeBetweenAcks < t )
            return NULL;

        if ( forceAckPreviousChunk )
        {
            // todo: send a complete ack for the previous chunk

            // todo: in order to do this, we need to know the number of slices in the previous chunk!

            // ...

            timeLastAckSent = t;
        }

        if ( receiving )
        {
            // todo: send an ack for the chunk currently being received

            timeLastAckSent = t;
        }

        return NULL;
    }

    const uint8_t* ReadChunk( uint16_t & chunkId, int & chunkSize )
    {
        if ( !readyToRead )
            return NULL;

        readyToRead = false;

        chunkId = this->chunkId - 1;        // because we already advanced it once all slices were received
        chunkSize = this->chunkSize;

        return chunkData;
    }
};

// todo: going to need a network simulator for this example (out of order, packet loss, latency...)

int main()
{
    srand( time( NULL ) );

    ChunkSender sender;
    ChunkReceiver receiver;

    // ...

    return 0;
}
