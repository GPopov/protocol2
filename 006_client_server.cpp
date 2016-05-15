/*
    Example source code for "Client/Server Connection"

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

#include "yojimbo.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace yojimbo;
using namespace protocol2;
using namespace network2;

//const uint32_t ProtocolId = 0x12341651;

const int MaxClients = 32;
const int ServerPort = 50000;
const int ClientPort = 60000;
const int ChallengeHashSize = 1031;                 // keep this prime
const float ChallengeSendRate = 0.1f;
const float ChallengeTimeOut = 10.0f;
/*
const float ConnectionTimeOut = 5.0f;
const float KeepAliveRate = 1.0f;
*/

uint64_t GenerateSalt()
{
    return ( ( uint64_t( rand() ) <<  0 ) & 0x000000000000FFFFull ) | 
           ( ( uint64_t( rand() ) << 16 ) & 0x00000000FFFF0000ull ) | 
           ( ( uint64_t( rand() ) << 32 ) & 0x0000FFFF00000000ull ) |
           ( ( uint64_t( rand() ) << 48 ) & 0xFFFF000000000000ull );
}

enum PacketTypes
{
    PACKET_CONNECTION_REQUEST,                      // client requests a connection.
    PACKET_CONNECTION_DENIED,                       // server denies client connection request.
    PACKET_CONNECTION_CHALLENGE,                    // server response to client connection request.
    PACKET_CONNECTION_RESPONSE,                     // client response to server connection challenge.
    /*
    PACKET_CONNECTION_KEEP_ALIVE,                   // keep alive packet sent at some low rate (once per-second) to keep the connection alive
    PACKET_CONNECTION_DISCONNECTED,                 // courtesy packet to indicate that the client has been disconnected. better than a timeout
    */
    NUM_CLIENT_SERVER_NUM_PACKETS
};

struct ConnectionRequestPacket : public Packet
{
    uint64_t client_salt;
    uint8_t data[256];

    ConnectionRequestPacket() : Packet( PACKET_CONNECTION_REQUEST )
    {
        client_salt = 0;
        memset( data, 0, sizeof( data ) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, client_salt );
        if ( Stream::IsReading && stream.GetBitsRemaining() < 256 * 8 )
            return false;
        serialize_bytes( stream, data, 256 );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

enum ConnectionDeniedReason
{
    CONNECTION_DENIED_SERVER_FULL,
    CONNECTION_DENIED_ALREADY_CONNECTED,
    CONNECTION_DENIED_NUM_VALUES
};

struct ConnectionDeniedPacket : public Packet
{
    uint64_t client_salt;
    ConnectionDeniedReason reason;

    ConnectionDeniedPacket() : Packet( PACKET_CONNECTION_DENIED )
    {
        client_salt = 0;
        reason = CONNECTION_DENIED_NUM_VALUES;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, client_salt );
        serialize_enum( stream, reason, ConnectionDeniedReason, CONNECTION_DENIED_NUM_VALUES );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ConnectionChallengePacket : public Packet
{
    uint64_t client_salt;
    uint64_t challenge_salt;

    ConnectionChallengePacket() : Packet( PACKET_CONNECTION_CHALLENGE )
    {
        client_salt = 0;
        challenge_salt = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, client_salt );
        serialize_uint64( stream, challenge_salt );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ConnectionResponsePacket : public Packet
{
    uint64_t client_salt;
    uint64_t challenge_salt;

    ConnectionResponsePacket() : Packet( PACKET_CONNECTION_RESPONSE )
    {
        client_salt = 0;
        challenge_salt = 0;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, client_salt );
        serialize_uint64( stream, challenge_salt );
        return true;
    }

    PROTOCOL2_DECLARE_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ClientServerPacketFactory : public PacketFactory
{
    ClientServerPacketFactory() : PacketFactory( NUM_CLIENT_SERVER_NUM_PACKETS ) {}

    Packet* CreateInternal( int type )
    {
        switch ( type )
        {
            case PACKET_CONNECTION_REQUEST:         return new ConnectionRequestPacket();
            case PACKET_CONNECTION_DENIED:          return new ConnectionDeniedPacket();
            case PACKET_CONNECTION_CHALLENGE:       return new ConnectionChallengePacket();
            case PACKET_CONNECTION_RESPONSE:        return new ConnectionResponsePacket();
            default:
                return NULL;
        }
    }
};

struct ServerChallengeEntry
{
    uint64_t client_salt;                           // random number generated by client and sent to server in connection request
    uint64_t challenge_salt;                        // random number generated by server and sent back to client in challenge packet
    double create_time;                             // time this challenge entry was created. used for challenge timeout
    double last_packet_send_time;                   // the last time we sent a challenge packet to this client
    Address address;                                // address the connection request came from
};

struct ServerChallengeHash
{
    int num_entries;
    uint8_t exists[ChallengeHashSize];
    ServerChallengeEntry entries[ChallengeHashSize];

    ServerChallengeHash() { memset( this, 0, sizeof( ServerChallengeHash ) ); }
};

uint64_t CalculateChallengeHashKey( const Address & address, uint64_t clientSalt, uint64_t serverSeed )
{
    char buffer[256];
    const char * addressString = address.ToString( buffer, sizeof( buffer ) );
    const int addressLength = strlen( addressString );
    return murmur_hash_64( &serverSeed, 8, murmur_hash_64( &clientSalt, 8, murmur_hash_64( addressString, addressLength, 0 ) ) );
}

struct Server
{
    NetworkInterface * m_networkInterface;                              // network interface for sending and receiving packets.

    uint64_t m_serverSalt;                                              // server salt. randomizes hash keys to eliminate challenge/response hash worst case attack.

    int m_numConnectedClients;                                          // number of connected clients
    
    bool m_clientConnected[MaxClients];                                 // true if client n is connected
    
    uint64_t m_clientSalt[MaxClients];                                  // array of client salt values per-client
    
    uint64_t m_challengeSalt[MaxClients];                               // array of challenge salt values per-client
    
    Address m_clientAddress[MaxClients];                                // array of client address values per-client
    
    double m_clientLastPacketReceiveTime[MaxClients];                   // last time a packet was received from a client (used for timeouts)

    ServerChallengeHash m_challengeHash;                                // challenge hash entries. stores client challenge/response data

protected:

    void ResetClientState( int clientIndex )
    {
        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );
        m_clientConnected[clientIndex] = false;
        m_clientSalt[clientIndex] = 0;
        m_challengeSalt[clientIndex] = 0;
        m_clientAddress[clientIndex] = Address();
        m_clientLastPacketReceiveTime[clientIndex] = -1000.0;           // IMPORTANT: avoid bad behavior near t=0.0
    }

    void ConnectClient( int clientIndex, const Address & address, uint64_t clientSalt, uint64_t challengeSalt )
    {
        assert( m_numConnectedClients >= 0 );
        assert( m_numConnectedClients < MaxClients - 1 );
        assert( !m_clientConnected[clientIndex] );
        m_numConnectedClients++;
        m_clientConnected[clientIndex] = true;
        m_clientSalt[clientIndex] = clientSalt;
        m_challengeSalt[clientIndex] = challengeSalt;
        m_clientAddress[clientIndex] = address;
    }

    void DisconnectClient( int /*clientIndex*/ )
    {
        // todo: implement
    }

    bool IsConnected( const Address & address ) const
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !m_clientConnected[i] )
                continue;
            if ( m_clientAddress[i] == address )
                return true;
        }
        return false;
    }

    ServerChallengeEntry * FindChallenge( const Address & address, uint64_t clientSalt, double time )
    {
        const uint64_t key = CalculateChallengeHashKey( address, clientSalt, m_serverSalt );

        int index = key % ChallengeHashSize;
        
        printf( "client salt = %llx\n", clientSalt );
        printf( "challenge hash key = %llx\n", key );
        printf( "challenge hash index = %d\n", index );

        // todo: check if it's timed out...
        if ( time < 0 )
            return NULL;

        if ( m_challengeHash.exists[index] && 
             m_challengeHash.entries[index].client_salt == clientSalt && 
             m_challengeHash.entries[index].address == address )
        {
            printf( "found challenge entry at index %d\n", index );

            return &m_challengeHash.entries[index];
        }

        return NULL;
    }

    ServerChallengeEntry * FindOrInsertChallenge( const Address & address, uint64_t clientSalt, double time )
    {
        const uint64_t key = CalculateChallengeHashKey( address, clientSalt, m_serverSalt );

        int index = key % ChallengeHashSize;
        
        printf( "client salt = %llx\n", clientSalt );
        printf( "challenge hash key = %llx\n", key );
        printf( "challenge hash index = %d\n", index );

        if ( !m_challengeHash.exists[index] || ( m_challengeHash.exists[index] && m_challengeHash.entries[index].create_time + ChallengeTimeOut < time ) )
        {
            printf( "found empty entry in challenge hash at index %d\n", index );

            ServerChallengeEntry * entry = &m_challengeHash.entries[index];

            entry->client_salt = clientSalt;
            entry->challenge_salt = GenerateSalt();
            entry->last_packet_send_time = time - ChallengeSendRate * 2;
            entry->create_time = time;
            entry->address = address;

            m_challengeHash.exists[index] = 1;

            return entry;
        }

        if ( m_challengeHash.exists[index] && 
             m_challengeHash.entries[index].client_salt == clientSalt && 
             m_challengeHash.entries[index].address == address )
        {
            printf( "found existing challenge hash entry at index %d\n", index );

            return &m_challengeHash.entries[index];
        }

        return NULL;
    }

public:

    Server( NetworkInterface & networkInterface )
    {
        m_networkInterface = &networkInterface;
        m_serverSalt = GenerateSalt();
        m_numConnectedClients = 0;
        for ( int i = 0; i < MaxClients; ++i )
            ResetClientState( i );
    }

    ~Server()
    {
        assert( m_networkInterface );
        m_networkInterface = NULL;
    }

    void ProcessConnectionRequest( const ConnectionRequestPacket & packet, const Address & address, double time )
    {
        char buffer[256];
        const char *addressString = address.ToString( buffer, sizeof( buffer ) );        
        printf( "processing connection request packet from: %s\n", addressString );

        if ( m_numConnectedClients == MaxClients )
        {
            printf( "connection denied: server is full\n" );
            ConnectionDeniedPacket * connectionDeniedPacket = (ConnectionDeniedPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DENIED );
            connectionDeniedPacket->client_salt = packet.client_salt;
            connectionDeniedPacket->reason = CONNECTION_DENIED_SERVER_FULL;
            m_networkInterface->SendPacket( address, connectionDeniedPacket );
            return;
        }

        if ( IsConnected( address ) )
        {
            printf( "connection denied: already connected\n" );
            ConnectionDeniedPacket * connectionDeniedPacket = (ConnectionDeniedPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DENIED );
            connectionDeniedPacket->client_salt = packet.client_salt;
            connectionDeniedPacket->reason = CONNECTION_DENIED_ALREADY_CONNECTED;
            m_networkInterface->SendPacket( address, connectionDeniedPacket );
            return;
        }

        ServerChallengeEntry * entry = FindOrInsertChallenge( address, packet.client_salt, time );
        if ( !entry )
            return;

        assert( entry );
        assert( entry->address == address );
        assert( entry->client_salt == packet.client_salt );

        if ( entry->last_packet_send_time + ChallengeSendRate < time )
        {
            printf( "sending connection challenge to %s (challenge salt = %llx)\n", addressString, entry->challenge_salt );
            ConnectionChallengePacket * connectionChallengePacket = (ConnectionChallengePacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DENIED );
            connectionChallengePacket->client_salt = packet.client_salt;
            connectionChallengePacket->challenge_salt = entry->challenge_salt;
            m_networkInterface->SendPacket( address, connectionChallengePacket );
            entry->last_packet_send_time = time;
        }
    }

    void ProcessConnectionResponse( const ConnectionResponsePacket & packet, const Address & address, double time )
    {
        if ( m_numConnectedClients == MaxClients )
        {
            printf( "connection denied: server is full\n" );
            ConnectionDeniedPacket * connectionDeniedPacket = (ConnectionDeniedPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DENIED );
            connectionDeniedPacket->client_salt = packet.client_salt;
            connectionDeniedPacket->reason = CONNECTION_DENIED_SERVER_FULL;
            m_networkInterface->SendPacket( address, connectionDeniedPacket );
            return;
        }

        ServerChallengeEntry * entry = FindOrInsertChallenge( address, packet.client_salt, time );
        if ( !entry )
            return;

        assert( entry );
        assert( entry->address == address );
        assert( entry->client_salt == packet.client_salt );

        if ( entry->challenge_salt != packet.challenge_salt )
        {
            printf( "challenge mismatch: expected %llx, got %llx\n", entry->challenge_salt, packet.challenge_salt );
            return;
        }

        // todo: implement FindFreeClientSlot and ConnectClient

        /*
        const int clientIndex = FindFreeClientSlot();

        assert( clientIndex != -1 );

        if ( clientIndex == -1 )
            return;

        ConnectClient( address, packet.client_salt, packet.challenge_salt, time );
        */
    }
};

enum ClientState
{
    CLIENT_STATE_DISCONNECTED,
    CLIENT_STATE_SENDING_CONNECTION_REQUEST,
    CLIENT_STATE_SENDING_CHALLENGE_RESPONSE,
    CLIENT_STATE_CONNECTED,
};

struct Client
{
    Address server_address;
    uint64_t server_guid;
    uint64_t client_guid;
};

int main()
{
    yojimbo::memory::initialize();

    srand( (unsigned int) time( NULL ) );

    printf( "client/server connection\n" );

    InitializeNetwork();

    Address clientAddress( "::1", ClientPort );
    Address serverAddress( "::1", ServerPort );

    ClientServerPacketFactory clientPacketFactory;
    ClientServerPacketFactory serverPacketFactory;

    SocketInterface clientInterface( yojimbo::memory::default_allocator(), clientPacketFactory, ClientPort );
    SocketInterface serverInterface( yojimbo::memory::default_allocator(), serverPacketFactory, ServerPort );

    if ( clientInterface.GetError() != SOCKET_ERROR_NONE || serverInterface.GetError() != SOCKET_ERROR_NONE )
        return 1;
    
    const int NumIterations = 30;

    double time = 0.0;

    const uint64_t client_salt = GenerateSalt();

    Server server( serverInterface );

    printf( "----------------------------------------------------------\n" );

    for ( int i = 0; i < NumIterations; ++i )
    {
        printf( "t = %f\n", time );

        if ( i <= 2 )
        {
            ConnectionRequestPacket packet;
            packet.client_salt = client_salt;

            server.ProcessConnectionRequest( packet, clientAddress, time );
        }

        /*
        char send_data[256];
        memset( send_data, 0, sizeof( send_data ) );
        clientSocket.SendPacket( serverAddress, send_data, sizeof( send_data ) );

        Address from;
        char recv_data[256];
        while ( int read_bytes = serverSocket.ReceivePacket( from, recv_data, sizeof( recv_data ) ) )
        {
            printf( "received packet: %d bytes\n", read_bytes );
        }
        */

        time += 0.1f;

        printf( "----------------------------------------------------------\n" );
    }

    ShutdownNetwork();

    yojimbo::memory::shutdown();

    return 0;
}
