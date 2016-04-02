/*
    Example source code for "Serialization Strategies"

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

#include "protocol2.h"
#include <stdio.h>
#include <time.h>

/*
template <typename Stream> bool serialize_vector_internal( Stream & stream, vec3f & vector )
{
    float values[3];
    if ( Stream::IsWriting )
        vector.store( values );
    serialize_float( stream, values[0] );
    serialize_float( stream, values[1] );
    serialize_float( stream, values[2] );
    if ( Stream::IsReading )
        vector.load( values );
    return true;
}

template <typename Stream> bool serialize_quaternion_internal( Stream & stream, quat4f & quaternion )
{
    float values[4];
    if ( Stream::IsWriting )
        quaternion.store( values );
    serialize_float( stream, values[0] );
    serialize_float( stream, values[1] );
    serialize_float( stream, values[2] );
    serialize_float( stream, values[3] );
    if ( Stream::IsReading )
        quaternion.load( values );
    return true;
}

template <typename Stream> bool serialize_compressed_vector_internal( Stream & stream, vec3f & vector )
{
    float values[3];
    if ( Stream::IsWriting )
        vector.store( values );
    serialize_float( stream, values[0] );
    serialize_float( stream, values[1] );
    serialize_float( stream, values[2] );
    if ( Stream::IsReading )
        vector.load( values );
    return true;
}
*/

template <int bits> struct compressed_quaternion
{
    enum { max_value = (1<<bits)-1 };

    uint32_t largest : 2;
    uint32_t integer_a : bits;
    uint32_t integer_b : bits;
    uint32_t integer_c : bits;

    void Load( float x, float y, float z, float w )
    {
        assert( bits > 1 );
        assert( bits <= 10 );

        const float minimum = - 1.0f / 1.414214f;       // 1.0f / sqrt(2)
        const float maximum = + 1.0f / 1.414214f;

        const float scale = float( ( 1 << bits ) - 1 );

        const float abs_x = fabs( x );
        const float abs_y = fabs( y );
        const float abs_z = fabs( z );
        const float abs_w = fabs( w );

        largest = 0;
        float largest_value = abs_x;

        if ( abs_y > largest_value )
        {
            largest = 1;
            largest_value = abs_y;
        }

        if ( abs_z > largest_value )
        {
            largest = 2;
            largest_value = abs_z;
        }

        if ( abs_w > largest_value )
        {
            largest = 3;
            largest_value = abs_w;
        }

        float a = 0;
        float b = 0;
        float c = 0;

        switch ( largest )
        {
            case 0:
                if ( x >= 0 )
                {
                    a = y;
                    b = z;
                    c = w;
                }
                else
                {
                    a = -y;
                    b = -z;
                    c = -w;
                }
                break;

            case 1:
                if ( y >= 0 )
                {
                    a = x;
                    b = z;
                    c = w;
                }
                else
                {
                    a = -x;
                    b = -z;
                    c = -w;
                }
                break;

            case 2:
                if ( z >= 0 )
                {
                    a = x;
                    b = y;
                    c = w;
                }
                else
                {
                    a = -x;
                    b = -y;
                    c = -w;
                }
                break;

            case 3:
                if ( w >= 0 )
                {
                    a = x;
                    b = y;
                    c = z;
                }
                else
                {
                    a = -x;
                    b = -y;
                    c = -z;
                }
                break;

            default:
                assert( false );
        }

        const float normal_a = ( a - minimum ) / ( maximum - minimum ); 
        const float normal_b = ( b - minimum ) / ( maximum - minimum );
        const float normal_c = ( c - minimum ) / ( maximum - minimum );

        integer_a = floor( normal_a * scale + 0.5f );
        integer_b = floor( normal_b * scale + 0.5f );
        integer_c = floor( normal_c * scale + 0.5f );
    }

    void Save( float & x, float & y, float & z, float & w ) const
    {
        // note: you're going to want to normalize the quaternion returned from this function

        assert( bits > 1 );
        assert( bits <= 10 );

        const float minimum = - 1.0f / 1.414214f;       // 1.0f / sqrt(2)
        const float maximum = + 1.0f / 1.414214f;

        const float scale = float( ( 1 << bits ) - 1 );

        const float inverse_scale = 1.0f / scale;

        const float a = integer_a * inverse_scale * ( maximum - minimum ) + minimum;
        const float b = integer_b * inverse_scale * ( maximum - minimum ) + minimum;
        const float c = integer_c * inverse_scale * ( maximum - minimum ) + minimum;

        switch ( largest )
        {
            case 0:
            {
                x = sqrtf( 1 - a*a - b*b - c*c );
                y = a;
                z = b;
                w = c;
            }
            break;

            case 1:
            {
                x = a;
                y = sqrtf( 1 - a*a - b*b - c*c );
                z = b;
                w = c;
            }
            break;

            case 2:
            {
                x = a;
                y = b;
                z = sqrtf( 1 - a*a - b*b - c*c );
                w = c;
            }
            break;

            case 3:
            {
                x = a;
                y = b;
                z = c;
                w = sqrtf( 1 - a*a - b*b - c*c );
            }
            break;

            default:
            {
                assert( false );
                x = 0;
                y = 0;
                z = 0;
                w = 1;
            }
        }
    }

    PROTOCOL2_SERIALIZE_FUNCTION( stream )
    {
        serialize_bits( stream, largest, 2 );
        serialize_bits( stream, integer_a, bits );
        serialize_bits( stream, integer_b, bits );
        serialize_bits( stream, integer_c, bits );
    }

    bool operator == ( const compressed_quaternion & other ) const
    {
        if ( largest != other.largest )
            return false;

        if ( integer_a != other.integer_a )
            return false;

        if ( integer_b != other.integer_b )
            return false;

        if ( integer_c != other.integer_c )
            return false;

        return true;
    }

    bool operator != ( const compressed_quaternion & other ) const
    {
        return ! ( *this == other );
    }
};


int main()
{
    // todo: this is a new example source code because the first article got too long. It's not coded yet, but check the next examples because they are.

    return 0;
}
