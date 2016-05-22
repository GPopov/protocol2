/*
    Functional Tests for Protocol2 Library and Network2 Library.

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

#include <sodium.h>

#if 0

packet 6: 00-00-02-fc-b8-13-34-00-ec-3d-c0-70-99-23-c2-10-ce-25-c4-d4-65-21-85-41-86-e9-82-61-f2-61-81-00-e0-77-d9-d3-e2-ac-08-99 (40 bytes)
send 6: 80-06-37-1b-ca-7f-ca-d7-2e-46-02-8b-ae-c8-ee-de-30-64-4c-e8-7a-ae-05-e3-c9-5f-e0-ac-a4-9c-d6-77-9b-98-31-cf-61-91-b3-39-f6-9b-c2-14-e8-de-7b-a7-ff-17-0a-35-c9-cd-f7-75 (56 bytes)
nonce 06-00-00-00-00-00-00-00 (8 bytes)
key 06-00-00-00-00-00-00-00-02-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-01-50-c3-00-00 (32 bytes)
recv 6: 80-06-37-1b-ca-7f-ca-d7-2e-46-02-8b-ae-c8-ee-de-30-64-4c-e8-7a-ae-05-e3-c9-5f-e0-ac-a4-9c-d6-77-9b-98-31-cf-61-91-b3-39-f6-9b-c2-14-e8-de-7b-a7-ff-17-0a-35-c9-cd-f7-75 (56 bytes)
decrypted 6: 80-06-02-fc-b8-13-34-00-ec-3d-c0-70-99-23-c2-10-ce-25-c4-d4-65-21-85-41-86-e9-82-61-f2-61-81-00-e0-77-31-cf-61-91-b3-39-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00 (56 bytes)
serialize check failed: 'end of TestPacketC'. expected d3d977e0, got cf3177e0

#endif

void test_packet_encryption()
{
    printf( "test_packet_encryption\n" );

    using namespace yojimbo;

    InitializeCrypto();

    uint8_t packet[1024];
  
    int packet_length = 1;
    memset( packet, 0, sizeof( packet ) );
    packet[0] = 1;  
  
    uint8_t key[KeyBytes];
    uint8_t nonce[NonceBytes];

    memset( key, 1, sizeof( key ) );
    memset( nonce, 1, sizeof( nonce ) );

    uint8_t encrypted_packet[2048];

    int encrypted_length;
    if ( !Encrypt( packet, packet_length, encrypted_packet, encrypted_length, nonce, key ) )
    {
        printf( "error: failed to encrypt\n" );
        exit(1);
    }

    const int expected_encrypted_length = 17;
    const uint8_t expected_encrypted_packet[] = { 0xfa, 0x6c, 0x91, 0xf7, 0xef, 0xdc, 0xed, 0x22, 0x09, 0x23, 0xd5, 0xbf, 0xa1, 0xe9, 0x17, 0x70, 0x14 };
    if ( encrypted_length != expected_encrypted_length || memcmp( expected_encrypted_packet, encrypted_packet, encrypted_length ) != 0 )
    {
        printf( "\npacket encryption failure!\n\n" );

        printf( " expected: " );
        PrintBytes( expected_encrypted_packet, expected_encrypted_length );
        printf( "\n" );

        printf( "      got: " );
        PrintBytes( encrypted_packet, encrypted_length );
        printf( "\n\n" );
    }

    uint8_t decrypted_packet[2048];
    int decrypted_length;
    if ( !Decrypt( encrypted_packet, encrypted_length, decrypted_packet, decrypted_length, nonce, key ) )
    {
        printf( "error: failed to decrypt\n" );
        exit(1);
    }

    if ( decrypted_length != packet_length || memcmp( packet, decrypted_packet, packet_length ) != 0 )
    {
        printf( "error: decrypted packet does not match original packet\n" );
        exit(1);
    }
}

int main()
{
    test_packet_encryption();
    return 0;
}
