/*
    Example source code for "Securing Dedicated Servers"

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
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <map>

using namespace std;
using namespace yojimbo;
using namespace protocol2;
using namespace network2;

const uint32_t ProtocolId = 0x12341651;

const int MaxClients = 64;
const int ClientPort = 40000;
const int ServerPort = 50000;
const float ConnectionRequestSendRate = 0.1f;
//const float ConnectionChallengeSendRate = 0.1f;
const float ConnectionResponseSendRate = 0.1f;
//const float ConnectionConfirmSendRate = 0.1f;
const float ConnectionHeartBeatRate = 1.0f;
const float ConnectionRequestTimeOut = 5.0f;
const float ChallengeResponseTimeOut = 5.0f;
const float ConnectionTimeOut = 10.0f;

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

struct ConnectToken
{
    uint32_t protocolId;                                                // the protocol id this connect token corresponds to.
 
    uint64_t clientId;                                                  // the unique client id. max one connection per-client id, per-server.
 
    uint64_t expiryTimestamp;                                           // timestamp the connect token expires (eg. ~10 seconds after token creation)
 
    int numServerAddresses;                                             // the number of server addresses in the connect token whitelist.
 
    Address serverAddresses[MaxServersPerConnectToken];                 // connect token only allows connection to these server addresses.
 
    uint8_t clientToServerKey[KeyBytes];                                // the key for encrypted communication from client -> server.
 
    uint8_t serverToClientKey[KeyBytes];                                // the key for encrypted communication from server -> client.

    ConnectToken()
    {
        protocolId = 0;
        clientId = 0;
        expiryTimestamp = 0;
        numServerAddresses = 0;
        memset( clientToServerKey, 0, sizeof( clientToServerKey ) );
        memset( serverToClientKey, 0, sizeof( serverToClientKey ) );
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
    {
        printf( "Encrypt_AEAD failed\n" );
        return false;
    }

    assert( encryptedLength == ConnectTokenBytes );

    return true;
}

bool DecryptConnectToken( const uint8_t * encryptedMessage, ConnectToken & decryptedToken, const uint8_t * additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    const int encryptedMessageLength = ConnectTokenBytes;

    uint64_t decryptedMessageLength;
    uint8_t decryptedMessage[ConnectTokenBytes];

    if ( !Decrypt_AEAD( encryptedMessage, encryptedMessageLength, decryptedMessage, decryptedMessageLength, additional, additionalLength, nonce, key ) )
    {
        printf( "Decrypt_AEAD failed\n" );
        return false;
    }

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

    ChallengeToken()
    {
        clientId = 0;
        memset( connectTokenMac, 0, sizeof( connectTokenMac ) );
        memset( clientToServerKey, 0, sizeof( clientToServerKey ) );
        memset( serverToClientKey, 0, sizeof( serverToClientKey ) );
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint64( stream, clientId );
        
        serialize_address( stream, clientAddress );

        serialize_address( stream, serverAddress );

        serialize_bytes( stream, connectTokenMac, MacBytes );

        serialize_bytes( stream, clientToServerKey, KeyBytes );

        serialize_bytes( stream, serverToClientKey, KeyBytes );

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
    {
        printf( "Encrypt_AEAD failed\n" );
        return false;
    }

    assert( encryptedLength == ChallengeTokenBytes );

    return true;
}

bool DecryptChallengeToken( const uint8_t * encryptedMessage, ChallengeToken & decryptedToken, const uint8_t * additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    const int encryptedMessageLength = ChallengeTokenBytes;

    uint64_t decryptedMessageLength;
    uint8_t decryptedMessage[ConnectTokenBytes];

    if ( !Decrypt_AEAD( encryptedMessage, encryptedMessageLength, decryptedMessage, decryptedMessageLength, additional, additionalLength, nonce, key ) )
    {
        printf( "Decrypt_AEAD failed\n" );
        return false;
    }

    assert( decryptedMessageLength == ChallengeTokenBytes - AuthBytes );

    ReadStream stream( decryptedMessage, ChallengeTokenBytes - AuthBytes );
    if ( !decryptedToken.Serialize( stream ) )
        return false;

    if ( stream.GetError() )
        return false;

    return true;
}

static uint8_t private_key[KeyBytes];

struct MatcherServerData
{
    Address serverAddress;                              // IP address of this server

    int numConnectedClients;                            // number of connected clients on this dedi

    uint64_t connectedClients[MaxClients];              // client ids connected to this server (tight array)
};

class Matcher
{
    uint64_t m_nonce;                                   // increments with each match request

    map<Address,MatcherServerData*> m_serverMap;        // maps network address to data for that server

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
    uint8_t challengeTokenData[ConnectTokenBytes];                        // encrypted challenge token data generated by matchmaker
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
    uint8_t challengeTokenData[ConnectTokenBytes];                        // encrypted challenge token data generated by matchmaker
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

struct ServerClientData
{
    Address address;
    uint64_t clientId;
    double connectTime;
    double lastPacketSendTime;
    double lastPacketReceiveTime;

    ServerClientData()
    {
        clientId = 0;
        connectTime = 0.0;
        lastPacketSendTime = 0.0;
        lastPacketReceiveTime = 0.0;
    }
};

class Server
{
    NetworkInterface * m_networkInterface;                              // network interface for sending and receiving packets.

    int m_numConnectedClients;                                          // number of connected clients
    
    bool m_clientConnected[MaxClients];                                 // true if client n is connected
    
    uint64_t m_clientId[MaxClients];                                    // array of client id values per-client
    
    Address m_clientAddress[MaxClients];                                // array of client address values per-client
    
    ServerClientData m_clientData[MaxClients];                          // heavier weight data per-client, eg. not for fast lookup

public:

    Server( NetworkInterface & networkInterface )
    {
        m_networkInterface = &networkInterface;
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

            if ( m_clientData[i].lastPacketSendTime + ConnectionHeartBeatRate > time )
                return;

            ConnectionHeartBeatPacket * packet = (ConnectionHeartBeatPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_HEARTBEAT );

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

                case PACKET_CONNECTION_HEARTBEAT:
                    ProcessConnectionHeartBeat( *(ConnectionHeartBeatPacket*)packet, address, time );
                    break;

                case PACKET_CONNECTION_DISCONNECT:
                    ProcessConnectionDisconnect( *(ConnectionDisconnectPacket*)packet, address, time );
                    break;

                default:
                    break;
            }

            m_networkInterface->DestroyPacket( packet );
        }
    }

    void CheckForTimeOut( double time )
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !m_clientConnected[i] )
                continue;

            if ( m_clientData[i].lastPacketReceiveTime + ConnectionTimeOut < time )
            {
                char buffer[256];
                const char *addressString = m_clientAddress[i].ToString( buffer, sizeof( buffer ) );
                printf( "client %d timed out (client address = %s, client id = %" PRIx64 ")\n", i, addressString, m_clientId[i] );
                DisconnectClient( i, time );
            }
        }
    }

protected:

    void ResetClientState( int clientIndex )
    {
        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );
        m_clientConnected[clientIndex] = false;
        m_clientId[clientIndex] = 0;
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

    int FindExistingClientIndex( const Address & address ) const
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( m_clientConnected[i] && m_clientAddress[i] == address )
                return i;
        }
        return -1;
    }

    void ConnectClient( int clientIndex, const Address & address, uint64_t clientId, double time )
    {
        assert( m_numConnectedClients >= 0 );
        assert( m_numConnectedClients < MaxClients - 1 );
        assert( !m_clientConnected[clientIndex] );

        m_numConnectedClients++;

        m_clientConnected[clientIndex] = true;
        m_clientId[clientIndex] = clientId;
        m_clientAddress[clientIndex] = address;

        m_clientData[clientIndex].address = address;
        m_clientData[clientIndex].clientId = clientId;
        m_clientData[clientIndex].connectTime = time;
        m_clientData[clientIndex].lastPacketSendTime = time;
        m_clientData[clientIndex].lastPacketReceiveTime = time;

        char buffer[256];
        const char *addressString = address.ToString( buffer, sizeof( buffer ) );
        printf( "client %d connected (client address = %s, client id = %" PRIx64 ")\n", clientIndex, addressString, clientId );

        ConnectionHeartBeatPacket * connectionHeartBeatPacket = (ConnectionHeartBeatPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_HEARTBEAT );

        SendPacketToConnectedClient( clientIndex, connectionHeartBeatPacket, time );
    }

    void DisconnectClient( int clientIndex, double time )
    {
        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );
        assert( m_numConnectedClients > 0 );
        assert( m_clientConnected[clientIndex] );

        char buffer[256];
        const char *addressString = m_clientAddress[clientIndex].ToString( buffer, sizeof( buffer ) );
        printf( "client %d disconnected: (client address = %s, client id = %" PRIx64 ")\n", clientIndex, addressString, m_clientId[clientIndex] );

        ConnectionDisconnectPacket * packet = (ConnectionDisconnectPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DISCONNECT );

        SendPacketToConnectedClient( clientIndex, packet, time );

        ResetClientState( clientIndex );

        m_numConnectedClients--;
    }

    bool IsConnected( uint64_t clientId ) const
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !m_clientConnected[i] )
                continue;
            if ( m_clientId[i] == clientId )
                return true;
        }
        return false;
    }

    bool IsConnected( const Address & address, uint64_t clientId ) const
    {
        for ( int i = 0; i < MaxClients; ++i )
        {
            if ( !m_clientConnected[i] )
                continue;
            if ( m_clientAddress[i] == address && m_clientId[i] == clientId )
                return true;
        }
        return false;
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

    void ProcessConnectionRequest( const ConnectionRequestPacket & /*packet*/, const Address & address, double /*time*/ )
    {
        char buffer[256];
        const char *addressString = address.ToString( buffer, sizeof( buffer ) );        
        printf( "processing connection request packet from: %s\n", addressString );

        if ( m_numConnectedClients == MaxClients )
        {
            printf( "connection denied: server is full\n" );
            ConnectionDeniedPacket * connectionDeniedPacket = (ConnectionDeniedPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DENIED );
            m_networkInterface->SendPacket( address, connectionDeniedPacket );
            return;
        }

        // todo

        /*

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
            printf( "sending connection challenge to %s (challenge salt = %" PRIx64 ")\n", addressString, entry->challenge_salt );
            ConnectionChallengePacket * connectionChallengePacket = (ConnectionChallengePacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_CHALLENGE );
            connectionChallengePacket->client_salt = packet.client_salt;
            connectionChallengePacket->challenge_salt = entry->challenge_salt;
            m_networkInterface->SendPacket( address, connectionChallengePacket );
            entry->last_packet_send_time = time;
        }
        */
    }

    void ProcessConnectionResponse( const ConnectionResponsePacket & /*packet*/, const Address & /*address*/, double /*time*/ )
    {
        // todo: no longer need client salt w. crypto

        /*
        const int existingClientIndex = FindExistingClientIndex( address, packet.client_salt, packet.challenge_salt );
        if ( existingClientIndex != -1 )
        {
            assert( existingClientIndex >= 0 );
            assert( existingClientIndex < MaxClients );

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
        printf( "processing connection response from client %s (client salt = %" PRIx64 ", challenge salt = %" PRIx64 ")\n", addressString, packet.client_salt, packet.challenge_salt );

        ServerChallengeEntry * entry = FindChallenge( address, packet.client_salt, time );
        if ( !entry )
            return;

        assert( entry );
        assert( entry->address == address );
        assert( entry->client_salt == packet.client_salt );

        if ( entry->challenge_salt != packet.challenge_salt )
        {
            printf( "connection challenge mismatch: expected %" PRIx64 ", got %" PRIx64 "\n", entry->challenge_salt, packet.challenge_salt );
            return;
        }

        if ( m_numConnectedClients == MaxClients )
        {
            if ( entry->last_packet_send_time + ConnectionChallengeSendRate < time )
            {
                printf( "connection denied: server is full\n" );
                ConnectionDeniedPacket * connectionDeniedPacket = (ConnectionDeniedPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DENIED );
                connectionDeniedPacket->reason = CONNECTION_DENIED_SERVER_FULL;
                m_networkInterface->SendPacket( address, connectionDeniedPacket );
                entry->last_packet_send_time = time;
            }
            return;
        }

        const int clientIndex = FindFreeClientIndex();

        assert( clientIndex != -1 );
        if ( clientIndex == -1 )
            return;

        ConnectClient( clientIndex, address, packet.client_salt, packet.challenge_salt, time );
        */
    }

    void ProcessConnectionHeartBeat( const ConnectionHeartBeatPacket & /*packet*/, const Address & address, double time )
    {
        const int clientIndex = FindExistingClientIndex( address );
        if ( clientIndex == -1 )
            return;

        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );

        m_clientData[clientIndex].lastPacketReceiveTime = time;
    }

    void ProcessConnectionDisconnect( const ConnectionDisconnectPacket & /*packet*/, const Address & address, double time )
    {
        const int clientIndex = FindExistingClientIndex( address );
        if ( clientIndex == -1 )
            return;

        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );

        DisconnectClient( clientIndex, time );
    }
};

class ClientServerNetworkInterface : public SocketInterface
{   
public:

    ClientServerNetworkInterface( ClientServerPacketFactory & packetFactory, uint16_t port ) : SocketInterface( memory_default_allocator(), packetFactory, ProtocolId, port )
    {
        EnablePacketEncryption();

        DisableEncryptionForPacketType( PACKET_CONNECTION_REQUEST );

        assert( IsEncryptedPacketType( PACKET_CONNECTION_REQUEST ) == false );
        assert( IsEncryptedPacketType( PACKET_CONNECTION_DENIED ) == true );
        assert( IsEncryptedPacketType( PACKET_CONNECTION_CHALLENGE ) == true );
        assert( IsEncryptedPacketType( PACKET_CONNECTION_RESPONSE ) == true );
        assert( IsEncryptedPacketType( PACKET_CONNECTION_HEARTBEAT ) == true );
        assert( IsEncryptedPacketType( PACKET_CONNECTION_DISCONNECT ) == true );
    }

    ~ClientServerNetworkInterface()
    {
        ClearSendQueue();
        ClearReceiveQueue();
    }
};

enum ClientState
{
    CLIENT_STATE_DISCONNECTED,
    CLIENT_STATE_SENDING_CONNECTION_REQUEST,
    CLIENT_STATE_SENDING_CHALLENGE_RESPONSE,
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT,
    CLIENT_STATE_CHALLENGE_RESPONSE_TIMED_OUT,
    CLIENT_STATE_CONNECTION_TIMED_OUT,
    CLIENT_STATE_CONNECTION_DENIED
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

    void Connect( const Address & address, double time )
    {
        Disconnect( time );
        // todo
//        m_clientSalt = GenerateSalt();
//        m_challengeSalt = 0;
        m_serverAddress = address;
        m_clientState = CLIENT_STATE_SENDING_CONNECTION_REQUEST;
        m_lastPacketSendTime = time - 1.0f;
        m_lastPacketReceiveTime = time;
    }

    bool IsConnecting() const
    {
        return m_clientState == CLIENT_STATE_SENDING_CONNECTION_REQUEST || m_clientState == CLIENT_STATE_SENDING_CHALLENGE_RESPONSE;
    }

    bool IsConnected() const
    {
        return m_clientState == CLIENT_STATE_CONNECTED;
    }

    bool ConnectionFailed() const
    {
        return m_clientState > CLIENT_STATE_CONNECTED;
    }

    void Disconnect( double time )
    {
        if ( m_clientState == CLIENT_STATE_CONNECTED )
        {
            printf( "client-side disconnect: (client salt = %" PRIx64 ", challenge salt = %" PRIx64 ")\n", m_clientSalt, m_challengeSalt );
            ConnectionDisconnectPacket * packet = (ConnectionDisconnectPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_DISCONNECT );
            SendPacketToServer( packet, time );
        }

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

                // todo: bunch of stuff!!!!!

                SendPacketToServer( packet, time );
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

                // todo: setup connection response
                //packet->challenge_salt = m_challengeSalt;
                
                SendPacketToServer( packet, time );
            }
            break;

            case CLIENT_STATE_CONNECTED:
            {
                if ( m_lastPacketSendTime + ConnectionHeartBeatRate > time )
                    return;

                ConnectionHeartBeatPacket * packet = (ConnectionHeartBeatPacket*) m_networkInterface->CreatePacket( PACKET_CONNECTION_HEARTBEAT );

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
                case PACKET_CONNECTION_DENIED:
                    ProcessConnectionDenied( *(ConnectionDeniedPacket*)packet, address, time );
                    break;

                case PACKET_CONNECTION_CHALLENGE:
                    ProcessConnectionChallenge( *(ConnectionChallengePacket*)packet, address, time );
                    break;

                case PACKET_CONNECTION_HEARTBEAT:
                    ProcessConnectionHeartBeat( *(ConnectionHeartBeatPacket*)packet, address, time );
                    break;

                case PACKET_CONNECTION_DISCONNECT:
                    ProcessConnectionDisconnect( *(ConnectionDisconnectPacket*)packet, address, time );
                    break;

                default:
                    break;
            }

            m_networkInterface->DestroyPacket( packet );
        }
    }

    void CheckForTimeOut( double time )
    {
        switch ( m_clientState )
        {
            case CLIENT_STATE_SENDING_CONNECTION_REQUEST:
            {
                if ( m_lastPacketReceiveTime + ConnectionRequestTimeOut < time )
                {
                    printf( "connection request to server timed out\n" );
                    m_clientState = CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT;
                    return;
                }
            }
            break;

            case CLIENT_STATE_SENDING_CHALLENGE_RESPONSE:
            {
                if ( m_lastPacketReceiveTime + ChallengeResponseTimeOut < time )
                {
                    printf( "challenge response to server timed out\n" );
                    m_clientState = CLIENT_STATE_CHALLENGE_RESPONSE_TIMED_OUT;
                    return;
                }
            }
            break;

            case CLIENT_STATE_CONNECTED:
            {
                if ( m_lastPacketReceiveTime + ConnectionTimeOut < time )
                {
                    printf( "keep alive timed out\n" );
                    m_clientState = CLIENT_STATE_CONNECTION_TIMED_OUT;
                    Disconnect( time );
                    return;
                }
            }
            break;

            default:
                break;
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

    void ProcessConnectionDenied( const ConnectionDeniedPacket & /*packet*/, const Address & address, double /*time*/ )
    {
        if ( m_clientState != CLIENT_STATE_SENDING_CONNECTION_REQUEST )
            return;

        if ( address != m_serverAddress )
            return;

        char buffer[256];
        const char * addressString = address.ToString( buffer, sizeof( buffer ) );
        printf( "client received connection denied from server: %s\n", addressString );
        m_clientState = CLIENT_STATE_CONNECTION_DENIED;
    }

    void ProcessConnectionChallenge( const ConnectionChallengePacket & /*packet*/, const Address & address, double time )
    {
        if ( m_clientState != CLIENT_STATE_SENDING_CONNECTION_REQUEST )
            return;

        if ( address != m_serverAddress )
            return;

        /*
        char buffer[256];
        const char * addressString = address.ToString( buffer, sizeof( buffer ) );
        */
        
        // todo: stuff

        //printf( "client received connection challenge from server: %s (challenge salt = %" PRIx64 ")\n", addressString, packet.challenge_salt );

        // todo: WUT

//        m_challengeSalt = packet.challenge_salt;

        m_clientState = CLIENT_STATE_SENDING_CHALLENGE_RESPONSE;

        m_lastPacketReceiveTime = time;
    }

    void ProcessConnectionHeartBeat( const ConnectionHeartBeatPacket & /*packet*/, const Address & address, double time )
    {
        if ( m_clientState < CLIENT_STATE_SENDING_CHALLENGE_RESPONSE )
            return;

        if ( address != m_serverAddress )
            return;

        if ( m_clientState == CLIENT_STATE_SENDING_CHALLENGE_RESPONSE )
        {
            char buffer[256];
            const char * addressString = address.ToString( buffer, sizeof( buffer ) );
            printf( "client is now connected to server: %s\n", addressString );
            m_clientState = CLIENT_STATE_CONNECTED;
        }

        m_lastPacketReceiveTime = time;
    }

    void ProcessConnectionDisconnect( const ConnectionDisconnectPacket & /*packet*/, const Address & address, double time )
    {
        if ( m_clientState != CLIENT_STATE_CONNECTED )
            return;

        if ( address != m_serverAddress )
            return;

        Disconnect( time );
    }
};

int main()
{
    printf( "\nsecuring dedicated servers\n\n" );

    Matcher matcher;

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

    printf( "requesting match\n\n" );

    if ( !matcher.RequestMatch( clientId, connectTokenData, connectTokenNonce, clientToServerKey, serverToClientKey, numServerAddresses, serverAddresses ) )
    {
        printf( "error: request match failed\n" );
        return 1;
    }

    printf( "connect token: " );
    PrintBytes( connectTokenData, ConnectTokenBytes );
    printf( "\n" );

    ConnectToken connectToken;
    if ( !DecryptConnectToken( connectTokenData, connectToken, NULL, 0, connectTokenNonce, private_key ) )
    {
        printf( "error: failed to decrypt connect token\n" );
        return 1;
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
        return 1;
    }

    if ( !EncryptChallengeToken( challengeToken, challengeTokenData, NULL, 0, challengeTokenNonce, private_key ) )
    {
        printf( "error: failed to encrypt challenge token\n" );
        return 1;
    }

    printf( "challenge token: " );
    PrintBytes( challengeTokenData, ChallengeTokenBytes );
    printf( "\n" );

    ChallengeToken decryptedChallengeToken;
    if ( !DecryptChallengeToken( challengeTokenData, decryptedChallengeToken, NULL, 0, challengeTokenNonce, private_key ) )
    {
        printf( "error: failed to decrypt challenge token\n" );
        return 1;
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

    memory_initialize();
    {
        srand( (unsigned int) time( NULL ) );

        InitializeNetwork();

        Address clientAddress( "::1", ClientPort );
        Address serverAddress( "::1", ServerPort );

        ClientServerPacketFactory packetFactory;

        ClientServerNetworkInterface clientInterface( packetFactory, ClientPort );
        ClientServerNetworkInterface serverInterface( packetFactory, ServerPort );

        // todo: switch over to interface with all packets set as encrypted

        if ( clientInterface.GetError() != SOCKET_ERROR_NONE || serverInterface.GetError() != SOCKET_ERROR_NONE )
        {
            printf( "error: failed to initialize sockets\n" );
            return 1;
        }
        
        const int NumIterations = 20;

        double time = 0.0;

        //Client client( clientInterface );

        Server server( serverInterface );
        
        //client.SecureConnect( serverAddress, clientId, connectToken, connectNonce, clientToServerKey, serverToClientKey );

        printf( "----------------------------------------------------------\n" );

        for ( int i = 0; i < NumIterations; ++i )
        {
            printf( "t = %f\n", time );

//            client.SendPackets( time );
            server.SendPackets( time );

//            clientInterface.WritePackets( time );
            serverInterface.WritePackets( time );

//            clientInterface.ReadPackets( time );
            serverInterface.ReadPackets( time );

//            client.ReceivePackets( time );
            server.ReceivePackets( time );

//            client.CheckForTimeOut( time );
            server.CheckForTimeOut( time );

/*
            if ( client.ConnectionFailed() )
            {
                printf( "error: client secure connect failed!\n" );
                break;
            }

            if ( client.IsConnected() )
                client.Disconnect( time );
*/

            time += 0.1f;

            printf( "----------------------------------------------------------\n" );
        }

        ShutdownNetwork();
    }

    memory_shutdown();

    printf( "\n" );

    return 0;
}
