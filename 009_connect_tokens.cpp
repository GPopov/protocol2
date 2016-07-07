/*
    Example source code for "Packet Encryption"

    Copyright © 2016, The Network Protocol Company, Inc.
    
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

#include "network2.h"
#include "protocol2.h"

#ifdef _MSC_VER
#define SODIUM_STATIC
#endif // #ifdef _MSC_VER

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

using namespace network2;
using namespace protocol2;

const uint32_t ProtocolId = 0x12398137;
const int ServerPort = 50000;
const int NonceBytes = 8;
const int KeyBytes = 32;
const int AuthBytes = 16;
const int ConnectTokenBytes = 1024;
const int MaxServersPerConnectToken = 8;
const int ConnectTokenExpirySeconds = 30;

void GenerateKey( uint8_t * key )
{
    assert( key );
    randombytes_buf( key, KeyBytes );
}

void RandomBytes( uint8_t * data, int bytes )
{
    assert( data );
    randombytes_buf( data, bytes );
}

bool Encrypt_AEAD( const uint8_t * message, uint64_t messageLength, 
                   uint8_t * encryptedMessage, uint64_t &  encryptedMessageLength,
                   const uint8_t * additional, uint64_t additionalLength,
                   const uint8_t * nonce,
                   const uint8_t * key )
{
    unsigned long long encryptedLength;

    int result = crypto_aead_chacha20poly1305_encrypt( encryptedMessage, &encryptedLength,
                                                       message, (unsigned long long) messageLength,
                                                       additional, (unsigned long long) additionalLength,
                                                       NULL, nonce, key );

    encryptedMessageLength = (uint64_t) encryptedLength;

    return result == 0;
}

bool Decrypt_AEAD( const uint8_t * encryptedMessage, uint64_t encryptedMessageLength, 
                   uint8_t * decryptedMessage, uint64_t & decryptedMessageLength,
                   const uint8_t * additional, uint64_t additionalLength,
                   const uint8_t * nonce,
                   const uint8_t * key )
{
    unsigned long long decryptedLength;

    int result = crypto_aead_chacha20poly1305_decrypt( decryptedMessage, &decryptedLength,
                                                       NULL,
                                                       encryptedMessage, (unsigned long long) encryptedMessageLength,
                                                       additional, (unsigned long long) additionalLength,
                                                       nonce, key );

    decryptedMessageLength = (uint64_t) decryptedLength;

    return result == 0;
}

const int MaxAddressLength = 256;

template <typename Stream> bool serialize_address_internal( Stream & stream, Address & address )
{
    char buffer[MaxAddressLength];

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

#define serialize_address( stream, value )                                              \
    do                                                                                  \
    {                                                                                   \
        if ( !serialize_address_internal( stream, value ) )                             \
            return false;                                                               \
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

    uint8_t random[KeyBytes];                                           // random data the client cannot possibly know.

    ConnectToken()
    {
        protocolId = 0;
        clientId = 0;
        expiryTimestamp = 0;
        numServerAddresses = 0;
        memset( clientToServerKey, 0, KeyBytes );
        memset( serverToClientKey, 0, KeyBytes );
        memset( random, 0, KeyBytes );
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

        serialize_bytes( stream, random, KeyBytes );

        return true;
    }

    bool operator == ( const ConnectToken & other ) const
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

        if ( memcmp( clientToServerKey, other.clientToServerKey, KeyBytes ) != 0 )
            return false;

        if ( memcmp( serverToClientKey, other.serverToClientKey, KeyBytes ) != 0 )
            return false;

        if ( memcmp( random, other.random, KeyBytes ) != 0 )
            return false;

        return true;
    }

    bool operator != ( const ConnectToken & other ) const
    {
        return ! ( (*this) == other );
    }
};

void GenerateConnectToken( ConnectToken & token, uint64_t clientId, int numServerAddresses, const Address * serverAddresses, uint32_t protocolId )
{
    uint64_t timestamp = (uint64_t) time( NULL );
    
    token.protocolId = protocolId;
    token.clientId = clientId;
    token.expiryTimestamp = timestamp + ConnectTokenExpirySeconds;
    
    assert( numServerAddresses > 0 );
    assert( numServerAddresses <= MaxServersPerConnectToken );
    token.numServerAddresses = numServerAddresses;
    for ( int i = 0; i < numServerAddresses; ++i )
        token.serverAddresses[i] = serverAddresses[i];

    GenerateKey( token.clientToServerKey );    

    GenerateKey( token.serverToClientKey );

    GenerateKey( token.random );
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
        return false;

    assert( encryptedLength == ConnectTokenBytes );

    return true;
}

bool DecryptConnectToken( const uint8_t * encryptedMessage, ConnectToken & decryptedToken, const uint8_t * additional, int additionalLength, const uint8_t * nonce, const uint8_t * key )
{
    const int encryptedMessageLength = ConnectTokenBytes;

    uint64_t decryptedMessageLength;
    uint8_t decryptedMessage[ConnectTokenBytes];

    if ( !Decrypt_AEAD( encryptedMessage, encryptedMessageLength, decryptedMessage, decryptedMessageLength, additional, additionalLength, nonce, key ) )
        return false;

    assert( decryptedMessageLength == ConnectTokenBytes - AuthBytes );

    ReadStream stream( decryptedMessage, ConnectTokenBytes - AuthBytes );
    if ( !decryptedToken.Serialize( stream ) )
        return false;

    if ( stream.GetError() )
        return false;

    return true;
}

void PrintBytes( const char * label, const uint8_t * data, int data_bytes )
{
    printf( "uint8_t %s[] = \n", label );
    printf( "{\n" );
    int counter = 0;
    for ( int i = 0; i < data_bytes; ++i )
    {
        if ( counter == 0 )
            printf( "    " );
        if ( i < data_bytes - 1 )
            printf( "0x%02x,", (int) data[i] );
        else
            printf( "0x%02x", (int) data[i] );
        if ( ++counter == 16 )
        {
            printf( "\n" );
            counter = 0;
        }
    }
    printf( "};\n\n" );
}

int main()
{
    printf( "\nconnect tokens\n\n" );

    assert( NonceBytes == crypto_aead_chacha20poly1305_NPUBBYTES );
    assert( KeyBytes == crypto_aead_chacha20poly1305_KEYBYTES );
    assert( AuthBytes == crypto_aead_chacha20poly1305_ABYTES );

    if ( sodium_init() != 0 )
    {
        printf( "error: failed to initialize sodium\n" );
        return 1;
    }

    uint8_t privateKey[KeyBytes];

    uint64_t clientId = 1;

    int numServerAddresses;
    Address serverAddresses[MaxServersPerConnectToken];

    GenerateKey( privateKey );

    PrintBytes( "privateKey", privateKey, KeyBytes );

    numServerAddresses = 1;
    serverAddresses[0] = Address( "127.0.0.1", ServerPort );

    ConnectToken connectToken;
    GenerateConnectToken( connectToken, clientId, numServerAddresses, serverAddresses, ProtocolId );

    printf( "generated connect token\n\n" );

    char serverAddressString[64];
    serverAddresses[0].ToString( serverAddressString, sizeof( serverAddressString ) );
    printf( "serverAddress = \"%s\"\n\n", serverAddressString );

    PrintBytes( "clientToServerKey", connectToken.clientToServerKey, KeyBytes );
    PrintBytes( "serverToClientKey", connectToken.serverToClientKey, KeyBytes );

    uint8_t connectTokenData[ConnectTokenBytes];
    uint8_t connectTokenNonce[NonceBytes];

    memset( connectTokenNonce, 0, sizeof( NonceBytes ) );

    if ( !EncryptConnectToken( connectToken, connectTokenData, NULL, 0, connectTokenNonce, privateKey ) )
    {
        printf( "error: failed to encrypt connect token\n" );
        return 1;
    }

    PrintBytes( "connectTokenData", connectTokenData, ConnectTokenBytes );

    ConnectToken decryptedConnectToken;

    if ( !DecryptConnectToken( connectTokenData, decryptedConnectToken, NULL, 0, connectTokenNonce, privateKey ) )
    {
        printf( "error: failed to decrypt connect token\n" );
        return 1;        
    }

    printf( "successfully decrypted connect token\n\n" );

    if ( connectToken != decryptedConnectToken )
    {
        printf( "error: decrypted connect token does not match\n" );
        return 1;
    }

    return 0;
}
