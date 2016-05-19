/*
    Yojimbo Client/Server Network Library.

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

#include "yojimbo_crypto.h"
#include <sodium.h>
#include <assert.h>

namespace yojimbo
{
    bool InitializeCrypto()
    {
        assert( NonceBytes == crypto_aead_chacha20poly1305_NPUBBYTES );
        assert( KeyBytes == crypto_aead_chacha20poly1305_KEYBYTES );
        assert( AuthBytes == crypto_aead_chacha20poly1305_ABYTES );
        assert( MacBytes == crypto_secretbox_MACBYTES );
        return sodium_init() == 0;
    }

    void GenerateKey( uint8_t * key )
    {
        assert( key );
        randombytes_buf( key, KeyBytes );
    }
    
    void GenerateNonce( uint8_t * nonce )
    {
        assert( nonce );
        randombytes_buf( nonce, NonceBytes );
    }

    bool Encrypt( const uint8_t * message, uint64_t messageLength, 
                  uint8_t * encryptedMessage, uint64_t & encryptedMessageLength,
                  const uint8_t * additional, uint64_t additionalLength,
                  const uint8_t * nonce,
                  const uint8_t * key )
    {
        int result = crypto_aead_chacha20poly1305_encrypt( encryptedMessage, &encryptedMessageLength,
                                                           message, messageLength,
                                                           additional, additionalLength,
                                                           NULL, nonce, key );

        return result == 0;
    }

    bool Decrypt( const uint8_t * encryptedMessage, uint64_t encryptedMessageLength, 
                  uint8_t * decryptedMessage, uint64_t & decryptedMessageLength,
                  const uint8_t * additional, uint64_t additionalLength,
                  const uint8_t * nonce,
                  const uint8_t * key )
    {
        int result = crypto_aead_chacha20poly1305_decrypt( decryptedMessage, &decryptedMessageLength,
                                                           NULL,
                                                           encryptedMessage, encryptedMessageLength,
                                                           additional, additionalLength,
                                                           nonce, key );

        return result == 0;
    }
}