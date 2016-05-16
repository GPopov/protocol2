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

#include "yojimbo.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace yojimbo;
using namespace protocol2;
using namespace network2;

const uint32_t ProtocolId = 0x12341651;

const int MaxClients = 32;
const int ServerPort = 50000;
const int ClientPort = 60000;
const int ChallengeHashSize = 1024;
const float ChallengeSendRate = 0.1f;
const float ChallengeTimeOut = 10.0f;
const float ConnectionRequestSendRate = 0.1f;
//const float ConnectionChallengeSendRate = 0.1f;
const float ConnectionResponseSendRate = 0.1f;
const float ConnectionConfirmSendRate = 0.1f;
const float ConnectionKeepAliveSendRate = 1.0f;
/*
const float ConnectionTimeOut = 5.0f;
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
    PACKET_CONNECTION_KEEP_ALIVE,                   // keep alive packet sent at some low rate (once per-second) to keep the connection alive
    /*
    PACKET_CONNECTION_DISCONNECTED,                 // courtesy packet to indicate that the client has been disconnected. better than a timeout
    */
    CLIENT_SERVER_NUM_PACKETS
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

struct ConnectionKeepAlivePacket : public Packet
{
    uint64_t client_salt;
    uint64_t challenge_salt;

    ConnectionKeepAlivePacket() : Packet( PACKET_CONNECTION_KEEP_ALIVE )
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
    ClientServerPacketFactory() : PacketFactory( CLIENT_SERVER_NUM_PACKETS ) {}

    Packet* CreateInternal( int type )
    {
        switch ( type )
        {
            case PACKET_CONNECTION_REQUEST:         return new ConnectionRequestPacket();
            case PACKET_CONNECTION_DENIED:          return new ConnectionDeniedPacket();
            case PACKET_CONNECTION_CHALLENGE:       return new ConnectionChallengePacket();
            case PACKET_CONNECTION_RESPONSE:        return new ConnectionResponsePacket();
            case PACKET_CONNECTION_KEEP_ALIVE:      return new ConnectionKeepAlivePacket();
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

struct ServerClientData
{
    Address address;
    uint64_t clientSalt;
    uint64_t challengeSalt;
    double connectTime;
    double lastPacketSendTime;
    double lastPacketReceiveTime;

    ServerClientData()
    {
        clientSalt = 0;
        challengeSalt = 0;
        connectTime = 0.0;
        lastPacketSendTime = 0.0;
        lastPacketReceiveTime = 0.0;
    }
};

class Server
{
    NetworkInterface * m_networkInterface;                              // network interface for sending and receiving packets.

    uint64_t m_serverSalt;                                              // server salt. randomizes hash keys to eliminate challenge/response hash worst case attack.

    int m_numConnectedClients;                                          // number of connected clients
    
    bool m_clientConnected[MaxClients];                                 // true if client n is connected
    
    uint64_t m_clientSalt[MaxClients];                                  // array of client salt values per-client

    uint64_t m_challengeSalt[MaxClients];                               // array of challenge salt values per-client
    
    Address m_clientAddress[MaxClients];                                // array of client address values per-client
    
    ServerClientData m_clientData[MaxClients];                          // heavier weight data per-client, eg. not for fast lookup

    ServerChallengeHash m_challengeHash;                                // challenge hash entries. stores client challenge/response data

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

    void SendPackets( double time )
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !m_clientConnected[i] )
                continue;

            if ( m_clientData[i].lastPacketSendTime + ConnectionKeepAliveSendRate > time )
                return;

            ConnectionKeepAlivePacket * packet = (ConnectionKeepAlivePacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_KEEP_ALIVE );
            packet->client_salt = m_clientSalt[i];
            packet->challenge_salt = m_challengeSalt[i];

            SendPacketToConnectedClient( i, packet, time );
        }
    }

    void ReceivePackets( double time )
    {
        while ( true )
        {
            Address address;
            Packet *packet = m_networkInterface->ReceivePacket( address );
            if ( !packet )
                break;
            
            switch ( packet->GetType() )
            {
                case PACKET_CONNECTION_REQUEST:
                    ProcessConnectionRequest( *(ConnectionRequestPacket*)packet, address, time );
                    break;

                case PACKET_CONNECTION_RESPONSE:
                    ProcessConnectionResponse( *(ConnectionResponsePacket*)packet, address, time );
                    break;

                // todo: process keepalive and use it update last received time from client

                default:
                    break;
            }

            m_networkInterface->DestroyPacket( packet );
        }
    }

protected:

    void ResetClientState( int clientIndex )
    {
        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );
        m_clientConnected[clientIndex] = false;
        m_clientSalt[clientIndex] = 0;
        m_challengeSalt[clientIndex] = 0;
        m_clientAddress[clientIndex] = Address();
        m_clientData[clientIndex] = ServerClientData();
    }

    int FindFreeClientIndex() const
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !m_clientConnected[i] )
                return i;
        }
        return -1;
    }

    int FindExistingClientIndex( const Address & address, uint64_t clientSalt ) const
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( m_clientConnected[i] && m_clientAddress[i] == address && m_clientSalt[i] == clientSalt )
                return i;
        }
        return -1;
    }

    int FindExistingClientIndex( const Address & address, uint64_t clientSalt, uint64_t challengeSalt ) const
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( m_clientConnected[i] && m_clientAddress[i] == address && m_clientSalt[i] == clientSalt && m_challengeSalt[i] == challengeSalt )
                return i;
        }
        return -1;
    }

    void ConnectClient( int clientIndex, const Address & address, uint64_t clientSalt, uint64_t challengeSalt, double time )
    {
        assert( m_numConnectedClients >= 0 );
        assert( m_numConnectedClients < MaxClients - 1 );
        assert( !m_clientConnected[clientIndex] );

        m_numConnectedClients++;

        m_clientConnected[clientIndex] = true;
        m_clientSalt[clientIndex] = clientSalt;
        m_challengeSalt[clientIndex] = challengeSalt;
        m_clientAddress[clientIndex] = address;

        m_clientData[clientIndex].address = address;
        m_clientData[clientIndex].clientSalt = clientSalt;
        m_clientData[clientIndex].challengeSalt = challengeSalt;
        m_clientData[clientIndex].connectTime = time;
        m_clientData[clientIndex].lastPacketSendTime = time;
        m_clientData[clientIndex].lastPacketReceiveTime = time;

        char buffer[256];
        const char *addressString = address.ToString( buffer, sizeof( buffer ) );
        printf( "client connected at client index %d (client address = %s, client salt = %llx, challenge salt = %llx)\n", clientIndex, addressString, clientSalt, challengeSalt );
    }

    void DisconnectClient( int /*clientIndex*/ )
    {
        // todo: implement
        assert( false );
    }

    bool IsConnected( const Address & address, uint64_t clientSalt ) const
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !m_clientConnected[i] )
                continue;
            if ( m_clientAddress[i] == address && m_clientSalt[i] == clientSalt )
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

    void SendPacketToConnectedClient( int clientIndex, Packet * packet, double time )
    {
        assert( packet );
        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );
        assert( m_clientConnected[clientIndex] );
        m_clientData[clientIndex].lastPacketSendTime = time;
        m_networkInterface->SendPacket( m_clientAddress[clientIndex], packet );
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

        if ( IsConnected( address, packet.client_salt ) )
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
            ConnectionChallengePacket * connectionChallengePacket = (ConnectionChallengePacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_CHALLENGE );
            connectionChallengePacket->client_salt = packet.client_salt;
            connectionChallengePacket->challenge_salt = entry->challenge_salt;
            m_networkInterface->SendPacket( address, connectionChallengePacket );
            entry->last_packet_send_time = time;
        }
    }

    void ProcessConnectionResponse( const ConnectionResponsePacket & packet, const Address & address, double time )
    {
        const int existingClientIndex = FindExistingClientIndex( address, packet.client_salt, packet.challenge_salt );
        if ( existingClientIndex != -1 )
        {
            if ( m_clientData[existingClientIndex].lastPacketSendTime + ConnectionConfirmSendRate < time )
            {
                ConnectionKeepAlivePacket * connectionKeepAlivePacket = (ConnectionKeepAlivePacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_KEEP_ALIVE );
                connectionKeepAlivePacket->client_salt = m_clientSalt[existingClientIndex];
                connectionKeepAlivePacket->challenge_salt = m_challengeSalt[existingClientIndex];
                SendPacketToConnectedClient( existingClientIndex, connectionKeepAlivePacket, time );
            }
            return;
        }

        char buffer[256];
        const char * addressString = address.ToString( buffer, sizeof( buffer ) );
        printf( "processing connection response from client %s (client salt = %llx, challenge salt = %llx)\n", addressString, packet.client_salt, packet.challenge_salt );

        ServerChallengeEntry * entry = FindChallenge( address, packet.client_salt, time );
        if ( !entry )
            return;

        assert( entry );
        assert( entry->address == address );
        assert( entry->client_salt == packet.client_salt );

        if ( entry->challenge_salt != packet.challenge_salt )
        {
            printf( "connection challenge mismatch: expected %llx, got %llx\n", entry->challenge_salt, packet.challenge_salt );
            return;
        }

        if ( m_numConnectedClients == MaxClients )
        {
            // todo: should have a rate limiter for this based on the entry
            printf( "connection denied: server is full\n" );
            ConnectionDeniedPacket * connectionDeniedPacket = (ConnectionDeniedPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DENIED );
            connectionDeniedPacket->client_salt = packet.client_salt;
            connectionDeniedPacket->reason = CONNECTION_DENIED_SERVER_FULL;
            m_networkInterface->SendPacket( address, connectionDeniedPacket );
            return;
        }

        const int clientIndex = FindFreeClientIndex();

        assert( clientIndex != -1 );
        if ( clientIndex == -1 )
            return;

        ConnectClient( clientIndex, address, packet.client_salt, packet.challenge_salt, time );

        ConnectionKeepAlivePacket * connectionKeepAlivePacket = (ConnectionKeepAlivePacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_KEEP_ALIVE );
        connectionKeepAlivePacket->client_salt = m_clientSalt[clientIndex];
        connectionKeepAlivePacket->challenge_salt = m_challengeSalt[clientIndex];
        SendPacketToConnectedClient( clientIndex, connectionKeepAlivePacket, time );
    }

    void ProcessConnectionKeepAlive( const ConnectionKeepAlivePacket & packet, const Address & address, double time )
    {
        const int clientIndex = FindExistingClientIndex( address, packet.client_salt, packet.challenge_salt );
        if ( clientIndex != -1 )
            return;

        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );

        m_clientData[clientIndex].lastPacketReceiveTime = time;
    }
};

enum ClientState
{
    CLIENT_STATE_DISCONNECTED,
    CLIENT_STATE_SENDING_CONNECTION_REQUEST,
    CLIENT_STATE_SENDING_CHALLENGE_RESPONSE,
    CLIENT_STATE_CONNECTED,
};

class Client
{
    ClientState m_clientState;                                          // current client state

    Address m_serverAddress;                                            // server address we are connecting or connected to.

    uint64_t m_clientSalt;                                              // client salt. randomly generated on each call to connect.

    uint64_t m_challengeSalt;                                           // challenge salt sent back from server in connection challenge.

    double m_lastPacketSendTime;                                        // time we last sent a packet to the server.

    double m_lastPacketReceiveTime;                                     // time we last received a packet from the server (used for timeouts).

    NetworkInterface * m_networkInterface;                              // network interface the client uses to send and receive packets.

public:

    Client( NetworkInterface & networkInterface )
    {
        m_networkInterface = &networkInterface;
        ResetConnectionData();
    }

    ~Client()
    {
        m_networkInterface = NULL;
    }

    void Connect( const Address & address )
    {
        Disconnect();
        m_clientSalt = GenerateSalt();
        m_challengeSalt = 0;
        m_serverAddress = address;
        m_clientState = CLIENT_STATE_SENDING_CONNECTION_REQUEST;
    }

    void Disconnect()
    {
        // todo: if connected, add pending connection to disconnect entries for clean shutdown

        ResetConnectionData();
    }

    void SendPackets( double time )
    {
        switch ( m_clientState )
        {
            case CLIENT_STATE_SENDING_CONNECTION_REQUEST:
            {
                if ( m_lastPacketSendTime + ConnectionRequestSendRate > time )
                    return;

                char buffer[256];
                const char *addressString = m_serverAddress.ToString( buffer, sizeof( buffer ) );
                printf( "client sending connection request to server: %s\n", addressString );

                ConnectionRequestPacket * packet = (ConnectionRequestPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_REQUEST );
                packet->client_salt = m_clientSalt;
                m_networkInterface->SendPacket( m_serverAddress, packet );
            }
            break;

            case CLIENT_STATE_SENDING_CHALLENGE_RESPONSE:
            {
                if ( m_lastPacketSendTime + ConnectionResponseSendRate > time )
                    return;

                char buffer[256];
                const char *addressString = m_serverAddress.ToString( buffer, sizeof( buffer ) );
                printf( "client sending challenge response to server: %s\n", addressString );

                ConnectionResponsePacket * packet = (ConnectionResponsePacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_RESPONSE );
                packet->client_salt = m_clientSalt;
                packet->challenge_salt = m_challengeSalt;

                SendPacketToServer( packet, time );
            }
            break;

            case CLIENT_STATE_CONNECTED:
            {
                if ( m_lastPacketSendTime + ConnectionKeepAliveSendRate > time )
                    return;

                ConnectionKeepAlivePacket * packet = (ConnectionKeepAlivePacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_KEEP_ALIVE );
                packet->client_salt = m_clientSalt;
                packet->challenge_salt = m_challengeSalt;

                SendPacketToServer( packet, time );
            }
            break;

            default:
                break;
        }
    }

    void ReceivePackets( double time )
    {
        while ( true )
        {
            Address address;
            Packet *packet = m_networkInterface->ReceivePacket( address );
            if ( !packet )
                break;
            
            switch ( packet->GetType() )
            {
                case PACKET_CONNECTION_CHALLENGE:
                    ProcessConnectionChallenge( *(ConnectionChallengePacket*)packet, address, time );
                    break;

                case PACKET_CONNECTION_KEEP_ALIVE:
                    ProcessConnectionKeepAlive( *(ConnectionKeepAlivePacket*)packet, address, time );
                    break;

                default:
                    break;
            }

            m_networkInterface->DestroyPacket( packet );
        }
    }

protected:

    void ResetConnectionData()
    {
        m_serverAddress = Address();
        m_clientState = CLIENT_STATE_DISCONNECTED;
        m_clientSalt = 0;
        m_challengeSalt = 0;
        m_lastPacketSendTime = -1000.0;
        m_lastPacketReceiveTime = -1000.0;
    }

    void SendPacketToServer( Packet *packet, double time )
    {
        assert( m_clientState != CLIENT_STATE_DISCONNECTED );
        assert( m_serverAddress.IsValid() );

        m_networkInterface->SendPacket( m_serverAddress, packet );

        m_lastPacketSendTime = time;
    }

    void ProcessConnectionChallenge( const ConnectionChallengePacket & packet, const Address & address, double /*time*/ )
    {
        if ( m_clientState != CLIENT_STATE_SENDING_CONNECTION_REQUEST )
            return;

        if ( packet.client_salt != m_clientSalt )
            return;

        char buffer[256];
        const char * addressString = address.ToString( buffer, sizeof( buffer ) );
        printf( "client received connection challenge from server: %s (challenge salt = %llx)\n", addressString, packet.challenge_salt );

        m_challengeSalt = packet.challenge_salt;

        m_clientState = CLIENT_STATE_SENDING_CHALLENGE_RESPONSE;

        // todo: set time last packet received (for timeout)
    }

    void ProcessConnectionKeepAlive( const ConnectionKeepAlivePacket & packet, const Address & address, double /*time*/ )
    {
        if ( m_clientState < CLIENT_STATE_SENDING_CHALLENGE_RESPONSE )
            return;

        if ( packet.client_salt != m_clientSalt )
            return;

        if ( m_clientState == CLIENT_STATE_SENDING_CHALLENGE_RESPONSE )
        {
            char buffer[256];
            const char * addressString = address.ToString( buffer, sizeof( buffer ) );
            printf( "client is now connected to server: %s\n", addressString );
            m_clientState = CLIENT_STATE_CONNECTED;
        }

        // todo: set time last packet received (for timeout)
    }
};

int main()
{
    printf( "client/server connection\n" );

    memory::initialize();
    {
        srand( (unsigned int) time( NULL ) );

        InitializeNetwork();

        Address clientAddress( "::1", ClientPort );
        Address serverAddress( "::1", ServerPort );

        ClientServerPacketFactory clientPacketFactory;
        ClientServerPacketFactory serverPacketFactory;

        SocketInterface clientInterface( memory::default_allocator(), clientPacketFactory, ProtocolId, ClientPort );
        SocketInterface serverInterface( memory::default_allocator(), serverPacketFactory, ProtocolId, ServerPort );

        if ( clientInterface.GetError() != SOCKET_ERROR_NONE || serverInterface.GetError() != SOCKET_ERROR_NONE )
            return 1;
        
        const int NumIterations = 20;

        double time = 0.0;

        Client client( clientInterface );

        Server server( serverInterface );
        
        client.Connect( serverAddress );

        printf( "----------------------------------------------------------\n" );

        for ( int i = 0; i < NumIterations; ++i )
        {
            printf( "t = %f\n", time );

            client.SendPackets( time );
            server.SendPackets( time );

            clientInterface.WritePackets( time );
            serverInterface.WritePackets( time );

            clientInterface.ReadPackets( time );
            serverInterface.ReadPackets( time );

            client.ReceivePackets( time );
            server.ReceivePackets( time );

            time += 0.1f;

            printf( "----------------------------------------------------------\n" );
        }

        ShutdownNetwork();
    }

    memory::shutdown();

    return 0;
}
