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

packet 8: 00-00-02-fc-b8-13-34-72-73-ea-c3-a0-55-cd-43-c4-fa-d4-c3-70-00-c7-83-01-6f-27-80-e1-04-03-84-00-e0-77-d9-d3-e2-ac-08-99 (40 bytes)
send 8: 80-08-b4-21-af-e7-cf-92-e5-44-d2-2f-4d-7e-39-6f-f8-ad-1a-63-5a-85-9d-c5-af-a7-10-ab-e5-9c-9f-e2-dc-73-4a-d5-30-ed-bf-77-99-f8-71-8b-3b-1b-17-a0-7a-42-b9-0c-12-4d-2a-62 (56 bytes)
nonce 08-00-00-00-00-00-00-00 (8 bytes)
key 08-00-00-00-00-00-00-00-02-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-01-50-c3-00-00 (32 bytes)
recv 8: 80-08-b4-21-af-e7-cf-92-e5-44-d2-2f-4d-7e-39-6f-f8-ad-1a-63-5a-85-9d-c5-af-a7-10-ab-e5-9c-9f-e2-dc-73-4a-d5-30-ed-bf-77-99-f8-71-8b-3b-1b-17-a0-7a-42-b9-0c-12-4d-2a-62 (56 bytes)
decrypted 8: 80-08-02-fc-b8-13-34-72-73-ea-c3-a0-55-cd-43-c4-fa-d4-c3-70-00-c7-83-01-6f-27-80-e1-04-03-84-00-e0-77-4a-d5-30-ed-bf-77 (40 bytes)
serialize check failed: 'end of TestPacketC'. expected d3d977e0, got d54a77e0

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
