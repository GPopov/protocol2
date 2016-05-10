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

#define PROTOCOL2_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//const uint32_t ProtocolId = 0x12341651;

const int MaxClients = 32;
/*
const int ServerPort = 50000;
const int ClientPort = 60000;
const float TimeOutInSeconds = 5.0f;
const float KeepAliveInSeconds = 1.0f;
*/

uint64_t GenerateSalt()
{
    // IMPORTANT: You should probably use a better random number generator!
    return ( ( (uint64_t) rand() <<  0 ) & 0x000000000000FFFFull ) | 
           ( ( (uint64_t) rand() << 16 ) & 0x00000000FFFF0000ull ) | 
           ( ( (uint64_t) rand() << 32 ) & 0x0000FFFF00000000ull ) |
           ( ( (uint64_t) rand() << 48 ) & 0xFFFF000000000000ull );
}

enum PacketTypes
{
    PACKET_CONNECTION_REQUEST,                      // client is requesting a connection.
    PACKET_CONNECTION_DENIED,                       // server denies client connection request with reason, eg. "game is full" etc.
    PACKET_CONNECTION_CHALLENGE,                    // server response to client connection request.
    PACKET_CONNECTION_CHALLENGE_RESPONSE,           // client response to server connection challenge.
    PACKET_CONNECTION_KEEP_ALIVE,                   // keep alive packet sent at some low rate (once per-second) to keep the connection alive
    PACKET_CONNECTION_DISCONNECTED,                 // courtesy packet to indicate that the client has been disconnected. better than a timeout
    NUM_CLIENT_SERVER_NUM_PACKETS
};

enum ServerClientState
{
    SERVER_CLIENT_STATE_DISCONNECTED,               // client slot is not connected
    SERVER_CLIENT_STATE_CHALLENGING,                // sending challenge response to client
    SERVER_CLIENT_STATE_CONNECTED,                  // server client slot is connected (keepalive is sent)
};

struct ServerContext
{
    uint64_t
    uint64_t client_salt[]
};

class Server
{
    uint64_t server_salt[MaxClients];               // per-client server-side generated salt to make it harder to spoof packets for other clients
    uint64_t client_salt[MaxClients];
    ServerClientState client_state[MaxClients];
    network2::Address client_address[MaxClients];
    double client_last_packet_time[MaxClients];

protected:

    void ResetClientState( int clientIndex )
    {
        assert( clientIndex >= 0 );
        assert( clientIndex < MaxClients );
        client_guid[clientIndex] = 0;
        client_state[clientIndex] = SERVER_CLIENT_STATE_DISCONNECTED;
        client_address[clientIndex] = network2::Address();
        client_last_packet_time[clientIndex] = 0.0;
    }

public:

    Server()
    {
        server_guid = GenerateGuid();
        for ( int i = 0; i < MaxClients; ++i )
            ResetClientState( i );
    }
};

enum ClientState
{
    CLIENT_STATE_DISCONNECTED,
    CLIENT_STATE_CONNECTING
    // ...
};

struct Client
{
    network2::Address server_address;
    uint64_t server_guid;
    uint64_t client_guid;
};

int main()
{
    srand( (unsigned int) time( NULL ) );

    printf( "client/server connection\n" );

    return 0;
}
