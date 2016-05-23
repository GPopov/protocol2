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
const int ServerPort = 50000;
const int TokenBytes = 1024;
const int MaxServersPerToken = 8;
const int TokenExpirySeconds = 10;

struct Token
{
    uint32_t protocolId;                                                // the protocol id this token corresponds to.
 
    uint64_t clientId;                                                  // the unique client id. max one connection per-client per-server.
 
    uint64_t expiryTimestamp;                                           // timestamp this token expires (eg. ~10 seconds after token creation)
 
    int numServerAddresses;                                             // the number of server addresses this token may be used on
 
    Address serverAddresses[MaxServersPerToken];                        // token only allows connection to this list of server addresses.
 
    uint8_t clientToServerKey[KeyBytes];                                // the key for encrypted communication from client -> server.
 
    uint8_t serverToClientKey[KeyBytes];                                // the key for encrypted communication from server -> client.

    Token()
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
        
        serialize_int( stream, numServerAddresses, 0, MaxServersPerToken - 1 );
        
        for ( int i = 0; i < numServerAddresses; ++i )
        {
            char buffer[64];
            if ( Stream::IsWriting )
            {
                assert( serverAddresses[i].IsValid() );
                serverAddresses[i].ToString( buffer, sizeof( buffer ) );
            }
            serialize_string( stream, buffer, sizeof( buffer ) );
            if ( Stream::IsReading )
            {
                serverAddresses[i] = Address( buffer );
                if ( !serverAddresses[i].IsValid() )
                    return false;
            }
        }

        serialize_bytes( stream, clientToServerKey, KeyBytes );

        serialize_bytes( stream, serverToClientKey, KeyBytes );

        return true;
    }

    bool operator == ( const Token & other ) const
    {
        if ( protocolId != other.protocolId )
            return false;

        if ( clientId != other.clientId )
            return false;

        if ( expiryTimestamp != other.expiryTimestamp )
            return false;

        if ( numServerAddresses != other.numServerAddresses )
            return false;

        for ( int i = 0; i < numServerAddresses; ++i )
        {
            if ( serverAddresses[i] != other.serverAddresses[i] )
                return false;
        }

        return true;
    }

    bool operator != ( const Token & other ) const
    {
        return !( (*this)== other );
    }
};

void GenerateToken( Token & token, uint64_t clientId, int numServerAddresses, const Address * serverAddresses )
{
    uint64_t timestamp = (uint64_t) time( NULL );

    token.protocolId = ProtocolId;
    token.clientId = clientId;
    token.expiryTimestamp = timestamp + TokenExpirySeconds;
    
    assert( numServerAddresses > 0 );
    assert( numServerAddresses <= MaxServersPerToken );
    token.numServerAddresses = numServerAddresses;
    for ( int i = 0; i < numServerAddresses; ++i )
        token.serverAddresses[i] = serverAddresses[i];

    GenerateKey( token.clientToServerKey );    

    GenerateKey( token.serverToClientKey );
}

bool EncryptToken( Token & token, uint8_t *encryptedMessage, const uint8_t *additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    uint8_t message[TokenBytes];
    memset( message, 0, TokenBytes );
    WriteStream stream( message, TokenBytes );
    if ( !token.Serialize( stream ) )
        return false;

    stream.Flush();
    
    if ( stream.GetError() )
        return false;

    uint64_t encryptedLength;

    if ( !Encrypt_AEAD( message, TokenBytes - AuthBytes, encryptedMessage, encryptedLength, additional, additionalLength, nonce, key ) )
    {
        printf( "Encrypt_AEAD failed\n" );
        return false;
    }

    assert( encryptedLength == TokenBytes );

    return true;
}

bool DecryptToken( const uint8_t * encryptedMessage, Token & decryptedToken, const uint8_t * additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    const int encryptedMessageLength = TokenBytes;

    uint64_t decryptedMessageLength;
    uint8_t decryptedMessage[TokenBytes];

    if ( !Decrypt_AEAD( encryptedMessage, encryptedMessageLength, decryptedMessage, decryptedMessageLength, additional, additionalLength, nonce, key ) )
    {
        printf( "Decrypt_AEAD failed\n" );
        return false;
    }

    assert( decryptedMessageLength == TokenBytes - AuthBytes );

    ReadStream stream( decryptedMessage, TokenBytes - AuthBytes );
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

        Token token;
        GenerateToken( token, clientId, numServerAddresses, serverAddresses );

        memcpy( clientToServerKey, token.clientToServerKey, KeyBytes );
        memcpy( serverToClientKey, token.serverToClientKey, KeyBytes );

        if ( !EncryptToken( token, tokenData, NULL, 0, (const uint8_t*) &m_nonce, private_key ) )
            return false;

        assert( NonceBytes == 8 );

        memcpy( tokenNonce, &m_nonce, NonceBytes );

        m_nonce++;

        return true;
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

int main()
{
    printf( "\nsecuring dedicated servers\n\n" );

    Matcher matcher;

    uint64_t clientId = 1;

    uint8_t tokenData[TokenBytes];
    uint8_t tokenNonce[NonceBytes];
    uint8_t clientToServerKey[KeyBytes];
    uint8_t serverToClientKey[KeyBytes];

    int numServerAddresses;
    Address serverAddresses[MaxServersPerToken];

    printf( "requesting match\n\n" );

    if ( !matcher.RequestMatch( clientId, tokenData, tokenNonce, clientToServerKey, serverToClientKey, numServerAddresses, serverAddresses ) )
    {
        printf( "error: request match failed\n" );
        return 1;
    }

    printf( "connect token: " );
    PrintBytes( tokenData, TokenBytes );
    printf( "\n" );

    Token token;
    if ( !DecryptToken( tokenData, token, NULL, 0, tokenNonce, private_key ) )
    {
        printf( "error: failed to decrypt token data\n" );
        return 1;
    }

    assert( token.clientId == 1 );
    assert( token.numServerAddresses == 1 );
    assert( token.serverAddresses[0] == Address( "::1", ServerPort ) );

    char serverAddressString[64];
    token.serverAddresses[0].ToString( serverAddressString, sizeof( serverAddressString ) );
    printf( "\nsuccess: token is valid for client %" PRIx64 " connection to %s\n\n", token.clientId, serverAddressString );

    return 0;
}
