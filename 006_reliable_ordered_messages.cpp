/*
    Example source code for "Reliable Ordered Messages"

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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#define NETWORK2_IMPLEMENTATION
#define PROTOCOL2_IMPLEMENTATION

#include "network2.h"
#include "protocol2.h"

using namespace protocol2;
using namespace network2;

//const uint32_t ProtocolId = 0x12341651;

class Message : public Object
{
public:

    Message( int type ) : m_refCount(1), m_id(0), m_type( type ) {}

    int GetId() const { return m_id; }

    int GetType() const { return m_type; }

    int GetRefCount() { return m_refCount; }

    virtual bool Serialize( ReadStream & stream ) = 0;

    virtual bool Serialize( WriteStream & stream ) = 0;

    virtual bool Serialize( MeasureStream & stream ) = 0;

protected:

    friend class MessageFactory;
  
    void AddRef() { m_refCount++; }

    void Release() { assert( m_refCount > 0 ); m_refCount--; }

    ~Message()
    {
        assert( m_refCount == 0 );
    }

private:

    Message( const Message & other );
    const Message & operator = ( const Message & other );

    int m_refCount;
    uint32_t m_id : 16;
    uint32_t m_type : 16;
};

class MessageFactory
{        
    int m_numTypes;

public:

    MessageFactory( int numTypes )
    {
        m_numTypes = numTypes;
    }

    Message * Create( int type )
    {
        assert( type >= 0 );
        assert( type < m_numTypes );
        return CreateInternal( type );
    }

    void AddRef( Message * message )
    {
        assert( message );        
        message->AddRef();
    }

    void Release( Message * message )
    {
        assert( message );
        message->Release();
        if ( message->GetRefCount() == 0 )
        {
            delete message;
        }
    }

    int GetNumTypes() const
    {
        return m_numTypes;
    }

protected:

    virtual Message * CreateInternal( int type ) = 0;
};

enum PacketTypes
{
    PACKET_CONNECTION,
    NUM_PACKETS
};

int main()
{
    printf( "\nreliable ordered messages\n\n" );

    // ...

    return 0;
}
