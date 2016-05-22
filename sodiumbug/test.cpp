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

----------------------------------------------------------
t = 0.700000
packet 7: 00-00-01-cb-b4-13-34-d0-4d-18-dd-31-8a-8d-d0-0c-1f-98-5f-d7-d8-22-c0-10-d3-7b-d9-d3-e2-ac-08-99 (32 bytes)
send 7: 80-07-c1-fc-52-10-9e-04-bd-64-fd-ac-8f-73-4f-21-56-af-94-4c-f0-b7-04-ab-d1-5a-0b-bb-9e-67-5f-03-a6-24-3e-46-b0-27-1a-c2-9a-c6-15-4a-ae-61-44-44 (48 bytes)
nonce 07-00-00-00-00-00-00-00 (8 bytes)
key ff-1b-c0-55-79-5f-af-60-71-ba-4a-c3-07-52-52-ec-af-26-40-39-76-88-cd-ae-89-98-b1-cb-cd-18-9a-c6 (32 bytes)
recv 7: 80-07-c1-fc-52-10-9e-04-bd-64-fd-ac-8f-73-4f-21-56-af-94-4c-f0-b7-04-ab-d1-5a-0b-bb-9e-67-5f-03-a6-24-3e-46-b0-27-1a-c2-9a-c6-15-4a-ae-61-44-44 (48 bytes)
decrypted 7: 80-07-01-cb-b4-13-34-d0-4d-18-dd-31-8a-8d-d0-0c-1f-98-5f-d7-d8-22-c0-10-d3-7b-d9-d3-e2-ac-08-99 (32 bytes)
server received packet type 1
----------------------------------------------------------

#endif

using namespace yojimbo;

int main()
{
    if ( !InitializeCrypto() )
    {
        printf( "failed to initialize crypto\n" );
        return 1;
    }

    const int packet_length = 32;

    uint8_t packet[1024] = { 0x00, 0x00, 0x01, 0xcb, 0xb4, 0x13, 0x34, 0xd0, 0x4d, 0x18, 0xdd, 0x31, 0x8a, 0x8d, 0xd0, 0x0c, 0x1f, 0x98, 0x5f, 0xd7, 0xd8, 0x22, 0xc0, 0x10, 0xd3, 0x7b, 0xd9, 0xd3, 0xe2, 0xac, 0x08, 0x99 };

    const int prefix = 2;
  
    printf( "packet " );
    PrintBytes( packet, packet_length );
    printf( "\n" );

    uint8_t key[] = { 0xff, 0x1b, 0xc0, 0x55, 0x79, 0x5f, 0xaf, 0x60, 
                      0x71, 0xba, 0x4a, 0xc3, 0x07, 0x52, 0x52, 0xec, 
                      0xaf, 0x26, 0x40, 0x39, 0x76, 0x88, 0xcd, 0xae, 
                      0x89, 0x98, 0xb1, 0xcb, 0xcd, 0x18, 0x9a, 0xc6 };

    uint8_t nonce[] = { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    uint8_t encrypted_packet[1024];
    memset( encrypted_packet, 0, sizeof( encrypted_packet ) );

    int encrypted_length;
    if ( !Encrypt( packet + prefix, packet_length - prefix, encrypted_packet + prefix, encrypted_length, nonce, key ) )
    {
        printf( "error: failed to encrypt\n" );
        return 1;
    }

    printf( "encrypted " );
    PrintBytes( encrypted_packet, encrypted_length + prefix );
    printf( "\n" );

    assert( encrypted_length + prefix == packet_length + MacBytes );

    uint8_t decrypted_packet[1024];
    memset( decrypted_packet, 0, sizeof( decrypted_packet ) );

    int decrypted_length;
    if ( !Decrypt( encrypted_packet + prefix, encrypted_length, decrypted_packet + prefix, decrypted_length, nonce, key ) )
    {
        printf( "error: failed to decrypt\n" );
        return 1;
    }

    printf( "decrypted " );
    PrintBytes( decrypted_packet, decrypted_length + prefix );
    printf( "\n" );

    assert( decrypted_length == packet_length - prefix );

    if ( memcmp( decrypted_packet, packet, packet_length ) == 0 )
    {
        printf( "test passed. bug not reproduced :(\n" );
    }
    else
    {
        printf( "bug reproduced!\n" );
    }

#if 0

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

#endif

    return 0;
}
