/*
    Network2 by Glenn Fiedler <glenn.fiedler@gmail.com>
    This software is in the public domain. Where that dedication is not recognized, 
    you are granted a perpetual, irrevocable license to copy, distribute, and modify this file as you see fit.
*/

#ifndef NETWORK2_H
#define NETWORK2_H

#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#define NETWORK2_SIMULATOR 1

namespace network2
{
    // todo: address class

    // todo: socket class

#if NETWORK2_SIMULATOR

    class Simulator
    {
        float latency;                                  // latency in milliseconds
        float jitter;                                   // jitter in milliseconds +/-
        float packetLoss;                               // packet loss percentage
        float duplicates;                               // duplicate packet percentage

        int numEntries;                                 // number of elements in the packet entry array.
        int currentIndex;                               // current index in the packet entry array. new packets are inserted here.

        struct Entry
        {
            double deliveryTime;                        // delivery time for this packet
            uint8_t *packetData;                        // packet data (takes ownership of pointer)
            int packetSize;                             // size of packet in bytes

            // todo: address
        };

        Entry *entries;                                 // pointer to dynamically allocated packet entries. this is where buffered packets are stored.

        double currentTime;                             // current time from last call to update. initially 0.0

    public:

        Simulator( int numPackets = 1024 );
        ~Simulator();

        void SetLatency( float milliseconds );
        void SetJitter( float milliseconds );
        void SetPacketLoss( float percent );
        void SetDuplicates( float percent );
        
        // todo: address
        void SendPacket( uint8_t *packetData, int packetSize );

        // todo: address
        uint8_t* ReceivePacket( int & packetSize );

        void Update( double t );
    };

#endif // #if NETWORK2_SIMULATOR
}

#endif // #ifndef NETWORK2_H

#ifdef NETWORK2_IMPLEMENTATION

namespace network2
{
#if NETWORK2_SIMULATOR

    Simulator::Simulator( int numPackets )
    {
        assert( numPackets > 0 );
        currentTime = 0.0;
        latency = 0.0f;
        jitter = 0.0f;
        packetLoss = 0.0f;
        duplicates = 0.0f;
        currentIndex = 0;
        numEntries = numPackets;
        entries = new Entry[numPackets];
        memset( entries, 0, sizeof( Entry ) * numPackets );
    }

    Simulator::~Simulator()
    {
        assert( numEntries > 0 );
        // todo: delete packet data
        /*
        for ( int i = 0; i < numEntries; ++i )
        {
            if ( entries[i].packet )
            {
                packetFactory->DestroyPacket( entries[i].packet );
            }
        }
        */
        delete [] entries;
        numEntries = 0;
    }

    void Simulator::SetLatency( float milliseconds )
    {
        latency = milliseconds;
    }

    void Simulator::SetJitter( float milliseconds )
    {
        jitter = milliseconds;
    }

    void Simulator::SetPacketLoss( float percent )
    {
        packetLoss = percent;
    }

    void Simulator::SetDuplicates( float percent )
    {
        duplicates = percent;
    }

    void Simulator::SendPacket( uint8_t *packetData, int packetSize )
    {
        // todo

        /*
        assert( packet );
        assert( packetFactory );

        if ( entries[currentIndex].packet )
        {
            packetFactory->DestroyPacket( entries[currentIndex].packet );
            memset( &entries[currentIndex], 0, sizeof( Entry ) );
        }

        // todo: packet loss

        double delay = latency;

        entries[currentIndex].packet = packet;
        entries[currentIndex].deliveryTime = currentTime + delay;

        currentIndex = ( currentIndex + 1 ) % numEntries;
        */
    }

    uint8_t* Simulator::ReceivePacket( int & packetSize )
    { 
        packetSize = 0;
        return NULL;

        /*
        int oldestEntryIndex = -1;
        double oldestEntryTime = 0;
        for ( int i = 0; i < numEntries; ++i )
        {
            if ( !entries[i].packet )
                continue;
            if ( oldestEntryIndex == -1 || entries[i].deliveryTime < oldestEntryTime )
            {
                oldestEntryIndex = i;
                oldestEntryTime = entries[i].deliveryTime;
            }
        }

        if ( oldestEntryIndex == -1 )
            return NULL;

        printf( "oldest entry index = %d\n", oldestEntryIndex );

        Packet *packet = entries[oldestEntryIndex].packet;

        memset( &entries[oldestEntryIndex], 0, sizeof( Entry ) );

        return packet;
        */
    }

    void Simulator::Update( double t )
    {
        currentTime = t;
    }

#endif // #if NETWORK2_SIMULATOR
}

#endif // #ifdef NETWORK2_IMPLEMENTATION
