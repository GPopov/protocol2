/*
    Protocol2 by Glenn Fiedler <glenn.fiedler@gmail.com>
    This software is in the public domain. Where that dedication is not recognized, 
    you are granted a perpetual, irrevocable license to copy, distribute, and modify this file as you see fit.
*/

#ifndef PROTOCOL2_H
#define PROTOCOL2_H

#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#define PROTOCOL2_SERIALIZE_CHECKS 1
#define PROTOCOL2_DEBUG_PACKET_LEAKS 1

#if PROTOCOL2_DEBUG_PACKET_LEAKS
#include <stdio.h>
#include <map>
#endif // #if PROTOCOL2_DEBUG_PACKET_LEAKS

#if    defined(__386__) || defined(i386)    || defined(__i386__)  \
    || defined(__X86)   || defined(_M_IX86)                       \
    || defined(_M_X64)  || defined(__x86_64__)                    \
    || defined(alpha)   || defined(__alpha) || defined(__alpha__) \
    || defined(_M_ALPHA)                                          \
    || defined(ARM)     || defined(_ARM)    || defined(__arm__)   \
    || defined(WIN32)   || defined(_WIN32)  || defined(__WIN32__) \
    || defined(_WIN32_WCE) || defined(__NT__)                     \
    || defined(__MIPSEL__)
  #define PROTOCOL2_LITTLE_ENDIAN 1
#else
  #define PROTOCOL2_BIG_ENDIAN 1
#endif

namespace protocol2
{
    template <typename T> const T & min( const T & a, const T & b )
    {
        return ( a < b ) ? a : b;
    }

    template <typename T> const T & max( const T & a, const T & b )
    {
        return ( a > b ) ? a : b;
    }

    template <typename T> T clamp( const T & value, const T & min, const T & max )
    {
        if ( value < min )
            return min;
        else if ( value > max )
            return max;
        else
            return value;
    }

    template <typename T> void swap( T & a, T & b )
    {
        T tmp = a;
        a = b;
        b = tmp;
    };

    template <typename T> T abs( const T & value )
    {
        return ( value < 0 ) ? -value : value;
    }

    template <uint32_t x> struct PopCount
    {
        enum {   a = x - ( ( x >> 1 )       & 0x55555555 ),
                 b =   ( ( ( a >> 2 )       & 0x33333333 ) + ( a & 0x33333333 ) ),
                 c =   ( ( ( b >> 4 ) + b ) & 0x0f0f0f0f ),
                 d =   c + ( c >> 8 ),
                 e =   d + ( d >> 16 ),

            result = e & 0x0000003f 
        };
    };

    template <uint32_t x> struct Log2
    {
        enum {   a = x | ( x >> 1 ),
                 b = a | ( a >> 2 ),
                 c = b | ( b >> 4 ),
                 d = c | ( c >> 8 ),
                 e = d | ( d >> 16 ),
                 f = e >> 1,

            result = PopCount<f>::result
        };
    };

    template <int64_t min, int64_t max> struct BitsRequired
    {
        static const uint32_t result = ( min == max ) ? 0 : ( Log2<uint32_t(max-min)>::result + 1 );
    };

    inline uint32_t popcount( uint32_t x )
    {
#ifdef __GNUC__
        return __builtin_popcount( x );
#else // #ifdef __GNUC__
        const uint32_t a = x - ( ( x >> 1 )       & 0x55555555 );
        const uint32_t b =   ( ( ( a >> 2 )       & 0x33333333 ) + ( a & 0x33333333 ) );
        const uint32_t c =   ( ( ( b >> 4 ) + b ) & 0x0f0f0f0f );
        const uint32_t d =   c + ( c >> 8 );
        const uint32_t e =   d + ( d >> 16 );
        const uint32_t result = e & 0x0000003f;
        return result;
#endif // #ifdef __GNUC__
    }

#ifdef __GNUC__

    inline int bits_required( uint32_t min, uint32_t max )
    {
        return 32 - __builtin_clz( max - min );
    }

#else // #ifdef __GNUC__

    inline uint32_t log2( uint32_t x )
    {
        const uint32_t a = x | ( x >> 1 );
        const uint32_t b = a | ( a >> 2 );
        const uint32_t c = b | ( b >> 4 );
        const uint32_t d = c | ( c >> 8 );
        const uint32_t e = d | ( d >> 16 );
        const uint32_t f = e >> 1;
        return popcount( f );
    }

    inline int bits_required( uint32_t min, uint32_t max )
    {
        return ( min == max ) ? 0 : log2( max - min ) + 1;
    }

#endif // #ifdef __GNUC__

    inline uint32_t host_to_network( uint32_t value )
    {
#if PROTOCOL2_BIG_ENDIAN
    #ifdef __GNUC__
        return __builtin_bswap32( value );
    #else // #ifdef __GNUC__
        // todo: need portable version
    #endif // #ifdef __GNUC__
#else // #if PROTOCOL2_BIG_ENDIAN
        return value;
#endif // #if PROTOCOL2_BIG_ENDIAN
    }

    inline uint32_t network_to_host( uint32_t value )
    {
#if PROTOCOL2_BIG_ENDIAN
    #ifdef __GNUC__
        return __builtin_bswap32( value );
    #else // #ifdef __GNUC__
        // todo: need portable version
    #endif // #ifdef __GNUC__
#else // #if PROTOCOL2_BIG_ENDIAN
        return value;
#endif // #if PROTOCOL2_BIG_ENDIAN
    }

    inline bool sequence_greater_than( uint16_t s1, uint16_t s2 )
    {
        return ( ( s1 > s2 ) && ( s1 - s2 <= 32768 ) ) || 
               ( ( s1 < s2 ) && ( s2 - s1  > 32768 ) );
    }

    inline bool sequence_less_than( uint16_t s1, uint16_t s2 )
    {
        return sequence_greater_than( s2, s1 );
    }

    inline int sequence_difference( uint16_t _s1, uint16_t _s2 )
    {
        int s1 = _s1;
        int s2 = _s2;
        if ( abs( s1 - s2 ) >= 32786 )
        {
            if ( s1 > s2 )
                s2 += 65536;
            else
                s1 += 65536;
        }
        return s1 - s2;
    }

    inline int signed_to_unsigned( int n )
    {
        return ( n << 1 ) ^ ( n >> 31 );
    }

    inline int unsigned_to_signed( uint32_t n )
    {
        return ( n >> 1 ) ^ ( -( n & 1 ) );
    }

    #define PROTOCOL2_STREAM_ERROR_NONE         0
    #define PROTOCOL2_STREAM_ERROR_OVERFLOW     1
    #define PROTOCOL2_STREAM_ERROR_INVALID      2
    #define PROTOCOL2_STREAM_ERROR_ABORTED      3

    class BitWriter
    {
    public:

        BitWriter( void* data, int bytes ) : m_data( (uint32_t*)data ), m_numWords( bytes / 4 )
        {
            assert( data );
            assert( ( bytes % 4 ) == 0 );           // buffer size must be a multiple of four
            m_numBits = m_numWords * 32;
            m_bitsWritten = 0;
            m_scratch = 0;
            m_bitIndex = 0;
            m_wordIndex = 0;
        }

        // todo: need "WouldOverflow( int bits )"

        void WriteBits( uint32_t value, int bits )
        {
            assert( bits > 0 );
            assert( bits <= 32 );
            assert( m_bitsWritten + bits <= m_numBits );

            value &= ( uint64_t( 1 ) << bits ) - 1;

            m_scratch |= uint64_t( value ) << ( 64 - m_bitIndex - bits );

            m_bitIndex += bits;

            if ( m_bitIndex >= 32 )
            {
                assert( m_wordIndex < m_numWords );
                m_data[m_wordIndex] = host_to_network( uint32_t( m_scratch >> 32 ) );
                m_scratch <<= 32;
                m_bitIndex -= 32;
                m_wordIndex++;
            }

            m_bitsWritten += bits;
        }

        void WriteAlign()
        {
            const int remainderBits = m_bitsWritten % 8;
            if ( remainderBits != 0 )
            {
                uint32_t zero = 0;
                WriteBits( zero, 8 - remainderBits );
                assert( m_bitsWritten % 8 == 0 );
            }
        }

        void WriteBytes( const uint8_t* data, int bytes )
        {
            assert( GetAlignBits() == 0 );
            assert( m_bitsWritten + bytes * 8 <= m_numBits );
            assert( m_bitIndex == 0 || m_bitIndex == 8 || m_bitIndex == 16 || m_bitIndex == 24 );

            int headBytes = ( 4 - m_bitIndex / 8 ) % 4;
            if ( headBytes > bytes )
                headBytes = bytes;
            for ( int i = 0; i < headBytes; ++i )
                WriteBits( data[i], 8 );
            if ( headBytes == bytes )
                return;

            assert( GetAlignBits() == 0 );

            int numWords = ( bytes - headBytes ) / 4;
            if ( numWords > 0 )
            {
                assert( m_bitIndex == 0 );
                memcpy( &m_data[m_wordIndex], data + headBytes, numWords * 4 );
                m_bitsWritten += numWords * 32;
                m_wordIndex += numWords;
                m_scratch = 0;
            }

            assert( GetAlignBits() == 0 );

            int tailStart = headBytes + numWords * 4;
            int tailBytes = bytes - tailStart;
            assert( tailBytes >= 0 && tailBytes < 4 );
            for ( int i = 0; i < tailBytes; ++i )
                WriteBits( data[tailStart+i], 8 );

            assert( GetAlignBits() == 0 );

            assert( headBytes + numWords * 4 + tailBytes == bytes );
        }

        void FlushBits()
        {
            if ( m_bitIndex != 0 )
            {
                assert( m_wordIndex < m_numWords );
                m_data[m_wordIndex++] = host_to_network( uint32_t( m_scratch >> 32 ) );
            }
        }

        int GetAlignBits() const
        {
            return ( 8 - m_bitsWritten % 8 ) % 8;
        }

        int GetBitsWritten() const
        {
            return m_bitsWritten;
        }

        int GetBitsAvailable() const
        {
            return m_numBits - m_bitsWritten;
        }

        const uint8_t* GetData() const
        {
            return (uint8_t*) m_data;
        }

        int GetBytesWritten() const
        {
            return ( m_bitsWritten / 8 ) + ( ( m_bitsWritten % 8 ) ? 1 : 0 );
        }

        int GetTotalBytes() const
        {
            return m_numWords * 4;              // todo: same here kinda
        }

    private:

        uint32_t* m_data;
        uint64_t m_scratch;
        int m_numBits;
        int m_numWords;
        int m_bitsWritten;
        int m_bitIndex;
        int m_wordIndex;
    };

    class BitReader
    {
    public:

        BitReader( const void* data, int bytes ) : m_data( (const uint32_t*)data ), m_numWords( bytes / 4 )
        {
            assert( data );
            assert( ( bytes % 4 ) == 0 );           // buffer size must be a multiple of four
            m_numBits = m_numWords * 32;
            m_bitsRead = 0;
            m_bitIndex = 0;
            m_wordIndex = 0;
            m_scratch = network_to_host( m_data[0] );
        }

        // todo: need function to check if n bits would overflow, e.g.: if ( WouldOverflow( bits ) )

        uint32_t ReadBits( int bits )
        {
            assert( bits > 0 );
            assert( bits <= 32 );
            assert( m_bitsRead + bits <= m_numBits );

            m_bitsRead += bits;

            assert( m_bitIndex < 32 );

            if ( m_bitIndex + bits < 32 )
            {
                m_scratch <<= bits;
                m_bitIndex += bits;
            }
            else
            {
                m_wordIndex++;
                assert( m_wordIndex < m_numWords );
                const uint32_t a = 32 - m_bitIndex;
                const uint32_t b = bits - a;
                m_scratch <<= a;
                m_scratch |= network_to_host( m_data[m_wordIndex] );
                m_scratch <<= b;
                m_bitIndex = b;
            }

            const uint32_t output = uint32_t( m_scratch >> 32 );

            m_scratch &= 0xFFFFFFFF;

            return output;
        }

        void ReadAlign()
        {
            const int remainderBits = m_bitsRead % 8;
            if ( remainderBits != 0 )
            {
                #ifdef NDEBUG
                ReadBits( 8 - remainderBits );
                #else
                uint32_t value = ReadBits( 8 - remainderBits );
                assert( value == 0 );
                assert( m_bitsRead % 8 == 0 );
                #endif
            }
        }

        void ReadBytes( uint8_t* data, int bytes )
        {
            assert( GetAlignBits() == 0 );
            assert( m_bitsRead + bytes * 8 <= m_numBits );
            assert( m_bitIndex == 0 || m_bitIndex == 8 || m_bitIndex == 16 || m_bitIndex == 24 );

            int headBytes = ( 4 - m_bitIndex / 8 ) % 4;
            if ( headBytes > bytes )
                headBytes = bytes;
            for ( int i = 0; i < headBytes; ++i )
                data[i] = ReadBits( 8 );
            if ( headBytes == bytes )
                return;

            assert( GetAlignBits() == 0 );

            int numWords = ( bytes - headBytes ) / 4;
            if ( numWords > 0 )
            {
                assert( m_bitIndex == 0 );
                memcpy( data + headBytes, &m_data[m_wordIndex], numWords * 4 );
                m_bitsRead += numWords * 32;
                m_wordIndex += numWords;
                m_scratch = network_to_host( m_data[m_wordIndex] );
            }

            assert( GetAlignBits() == 0 );

            int tailStart = headBytes + numWords * 4;
            int tailBytes = bytes - tailStart;
            assert( tailBytes >= 0 && tailBytes < 4 );
            for ( int i = 0; i < tailBytes; ++i )
                data[tailStart+i] = ReadBits( 8 );

            assert( GetAlignBits() == 0 );

            assert( headBytes + numWords * 4 + tailBytes == bytes );
        }

        int GetAlignBits() const
        {
            return ( 8 - m_bitsRead % 8 ) % 8;
        }

        int GetBitsRead() const
        {
            return m_bitsRead;
        }

        int GetBytesRead() const
        {
            return ( m_wordIndex + 1 ) * 4;
        }

        int GetBitsRemaining() const
        {
            return m_numBits - m_bitsRead;
        }

        int GetTotalBits() const 
        {
            return m_numBits;
        }

        int GetTotalBytes() const
        {
            // todo: this is completely fucking wrong!
            return m_numBits * 8;
        }

    private:

        const uint32_t* m_data;
        uint64_t m_scratch;
        int m_numBits;
        int m_numWords;
        int m_bitsRead;
        int m_bitIndex;
        int m_wordIndex;
    };

    class WriteStream
    {
    public:

        enum { IsWriting = 1 };
        enum { IsReading = 0 };

        WriteStream( uint8_t* buffer, int bytes ) : m_writer( buffer, bytes ), m_error( PROTOCOL2_STREAM_ERROR_NONE ), m_context( NULL ) {}

        bool SerializeInteger( int32_t value, int32_t min, int32_t max )
        {
            assert( min < max );
            assert( value >= min );
            assert( value <= max );
            if ( GetError() )
                return false;
            const int bits = bits_required( min, max );
            uint32_t unsigned_value = value - min;
            m_writer.WriteBits( unsigned_value, bits );
            return true;
        }

        // todo: need a version with min/max and bits required to serialize so values can be checked for compile time bits required
        // todo: make sure this version checks that the bits are enough to handle the min/max (assert only)

        bool SerializeBits( uint32_t value, int bits )
        {
            assert( bits > 0 );
            assert( bits <= 32 );
            // todo: detect error, return false
            // todo: check for overflow, return false
            m_writer.WriteBits( value, bits );
            return true;
        }

        bool SerializeBytes( const uint8_t* data, int bytes )
        {
            assert( data );
            assert( bytes >= 0 );
            if ( !SerializeAlign() )
                return false;
            m_writer.WriteBytes( data, bytes );
            return true;
        }

        bool SerializeAlign()
        {
            if ( GetError() )
                return false;
            m_writer.WriteAlign();
            return true;
        }

        int GetAlignBits() const
        {
            return m_writer.GetAlignBits();
        }

        bool SerializeCheck( uint32_t magic )
        {
#if PROTOCOL2_SERIALIZE_CHECKS
            SerializeAlign();
            SerializeBits( magic, 32 );
#endif // #if PROTOCOL2_SERIALIZE_CHECKS
            return true;
        }

        void Flush()
        {
            m_writer.FlushBits();
        }

        const uint8_t* GetData() const
        {
            return m_writer.GetData();
        }

        int GetBytesProcessed() const
        {
            return m_writer.GetBytesWritten();
        }

        int GetBitsProcessed() const
        {
            return m_writer.GetBitsWritten();
        }

        int GetBitsRemaining() const
        {
            return GetTotalBits() - GetBitsProcessed();
        }

        int GetTotalBits() const
        {
            return m_writer.GetTotalBytes() * 8;
        }

        int GetTotalBytes() const
        {
            return m_writer.GetTotalBytes();
        }

        void SetContext( void *context )
        {
            m_context = context;
        }

        void* GetContext() const
        {
            return m_context;
        }

        void Abort()
        {
            m_error = PROTOCOL2_STREAM_ERROR_ABORTED;
        }

        int GetError() const
        {
            return m_error;
        }

    private:

        int m_error;
        void *m_context;
        BitWriter m_writer;
    };

    class ReadStream
    {
    public:

        enum { IsWriting = 0 };
        enum { IsReading = 1 };

        ReadStream( const uint8_t* buffer, int bytes ) : m_bitsRead(0), m_reader( buffer, bytes ), m_context( NULL ), m_error( PROTOCOL2_STREAM_ERROR_NONE ) {}

        bool SerializeInteger( int32_t & value, int32_t min, int32_t max )
        {
            assert( min < max );
            // todo: check for existing error
            const int bits = bits_required( min, max );
            // todo: check for overflow
            uint32_t unsigned_value = m_reader.ReadBits( bits );
            value = (int32_t) unsigned_value + min;
            m_bitsRead += bits;
            return true;
        }

        // todo: need version that takes min,max and bits

        bool SerializeBits( uint32_t & value, int bits )
        {
            assert( bits > 0 );
            assert( bits <= 32 );
            // todo: check for existing error
            // todo: check for overflow
            uint32_t read_value = m_reader.ReadBits( bits );
            value = read_value;
            m_bitsRead += bits;
            return true;
        }

        bool SerializeBytes( uint8_t* data, int bytes )
        {
            // todo: check for existing error
            // todo: check for overflow
            SerializeAlign();
            m_reader.ReadBytes( data, bytes );
            m_bitsRead += bytes * 8;
            return true;
        }

        bool SerializeAlign()
        {
            // todo: check for existing error
            // todo: check for overflow
            m_reader.ReadAlign();
            return true;
        }

        int GetAlignBits() const
        {
            return m_reader.GetAlignBits();
        }

        bool SerializeCheck( uint32_t magic )
        {
#if PROTOCOL2_SERIALIZE_CHECKS            
            SerializeAlign();
            uint32_t value = 0;
            SerializeBits( value, 32 );
            // todo: this should not just return false, it should also set error INVALID
            assert( value == magic );
            return value == magic;
#else // #if PROTOCOL2_SERIALZE_CHECKS
            return true;
#endif // #if PROTOCOL2_SERIALIZE_CHECKS
        }

        int GetBitsProcessed() const
        {
            return m_bitsRead;
        }

        int GetBytesProcessed() const
        {
            return m_bitsRead / 8 + ( m_bitsRead % 8 ? 1 : 0 );
        }

        void SetContext( void* context )
        {
            m_context = context;
        }

        void* GetContext() const
        {
            return m_context;
        }

        void Abort()
        {
            m_error = PROTOCOL2_STREAM_ERROR_ABORTED;
        }

        int GetError() const
        {
            return m_error;
        }

        int GetBytesRead() const
        {
            return m_reader.GetBytesRead();
        }

    private:

        void* m_context;
        int m_error;
        int m_bitsRead;
        BitReader m_reader;
    };

    class MeasureStream
    {
    public:

        enum { IsWriting = 1 };
        enum { IsReading = 0 };

        MeasureStream( int bytes ) : m_totalBytes( bytes ), m_bitsWritten(0), m_context( NULL ), m_error( PROTOCOL2_STREAM_ERROR_NONE ) {}

        bool SerializeInteger( int32_t value, int32_t min, int32_t max )
        {
            assert( min < max );
            assert( value >= min );
            assert( value <= max );
            const int bits = bits_required( min, max );
            m_bitsWritten += bits;
            return true;
        }

        bool SerializeBits( uint32_t value, int bits )
        {
            assert( bits > 0 );
            assert( bits <= 32 );
            m_bitsWritten += bits;
            return true;
        }

        void SerializeBytes( const uint8_t* data, int bytes )
        {
            SerializeAlign();
            m_bitsWritten += bytes * 8;
        }

        void SerializeAlign()
        {
            const int alignBits = GetAlignBits();
            m_bitsWritten += alignBits;
        }

        int GetAlignBits() const
        {
            return 7;       // we can't know for sure, so be conservative and assume worst case
        }

        bool SerializeCheck( uint32_t magic )
        {
#if PROTOCOL2_SERIALIZE_CHECKS
            SerializeAlign();
            m_bitsWritten += 32;
#endif // #if PROTOCOL2_SERIALIZE_CHECKS
            return true;
        }

        int GetBitsProcessed() const
        {
            return m_bitsWritten;
        }

        int GetBytesProcessed() const
        {
            return m_bitsWritten / 8 + ( ( m_bitsWritten % 8 ) ? 1 : 0 );
        }

        int GetTotalBytes() const
        {
            return m_totalBytes;
        }

        int GetTotalBits() const
        {
            return m_totalBytes * 8;
        }

        void SetContext( void* context )
        {
            m_context = context;
        }

        void* GetContext() const
        {
            return m_context;
        }

        void Abort()
        {
            m_error = PROTOCOL2_STREAM_ERROR_ABORTED;
        }

        int GetError() const
        {
            return m_error;
        }

    private:

        void* m_context;
        int m_error;
        int m_totalBytes;
        int m_bitsWritten;
    };

    template <typename T> bool serialize_object( ReadStream & stream, T & object )
    {                        
        return object.SerializeRead( stream );
    }

    template <typename T> bool serialize_object( WriteStream & stream, T & object )
    {                        
        return object.SerializeWrite( stream );
    }

    template <typename T> bool serialize_object( MeasureStream & stream, T & object )
    {                        
        return object.SerializeMeasure( stream );
    }

    // todo: we need to clamp on read below, as well as error the stream (returns 0 on error?)

    #define serialize_int( stream, value, min, max )                    \
        do                                                              \
        {                                                               \
            assert( min < max );                                        \
            int32_t int32_value;                                        \
            if ( Stream::IsWriting )                                    \
            {                                                           \
                assert( value >= min );                                 \
                assert( value <= max );                                 \
                int32_value = (int32_t) value;                          \
            }                                                           \
            if ( !stream.SerializeInteger( int32_value, min, max ) )    \
                return false;                                           \
            if ( Stream::IsReading )                                    \
            {                                                           \
                value = int32_value;                                    \
                assert( value >= min );                                 \
                assert( value <= max );                                 \
            }                                                           \
        } while (0)

    #define serialize_bits( stream, value, bits )                       \
        do                                                              \
        {                                                               \
            assert( bits > 0 );                                         \
            assert( bits <= 32 );                                       \
            uint32_t uint32_value;                                      \
            if ( Stream::IsWriting )                                    \
                uint32_value = (uint32_t) value;                        \
            if ( !stream.SerializeBits( uint32_value, bits ) )          \
                return false;                                           \
            if ( Stream::IsReading )                                    \
                value = uint32_value;                                   \
        } while (0)

    #define serialize_bool( stream, value ) serialize_bits( stream, value, 1 )

    template <typename Stream> bool serialize_float_internal( Stream & stream, float & value )
    {
        union FloatInt
        {
            float float_value;
            uint32_t int_value;
        };

        FloatInt tmp;
        if ( Stream::IsWriting )
            tmp.float_value = value;

        bool result = stream.SerializeBits( tmp.int_value, 32 );

        if ( Stream::IsReading )
            value = tmp.float_value;

        return result;
    }

    #define serialize_float( stream, value )                                        \
        do                                                                          \
        {                                                                           \
            if ( !protocol2::serialize_float_internal( stream, value ) )            \
                return false;                                                       \
        } while (0)


    // todo: have to use defines for these. can't do the return false thing otherwise

    /*
    template <typename Stream> void serialize_uint16( Stream & stream, uint16_t & value )
    {
        serialize_bits( stream, value, 16 );
    }

    template <typename Stream> void serialize_uint32( Stream & stream, uint32_t & value )
    {
        serialize_bits( stream, value, 32 );
    }

    template <typename Stream> void serialize_uint64( Stream & stream, uint64_t & value )
    {
        uint32_t hi,lo;
        if ( Stream::IsWriting )
        {
            lo = value & 0xFFFFFFFF;
            hi = value >> 32;
        }
        serialize_bits( stream, lo, 32 );
        serialize_bits( stream, hi, 32 );
        if ( Stream::IsReading )
            value = ( uint64_t(hi) << 32 ) | lo;
    }

    template <typename Stream> void serialize_int16( Stream & stream, int16_t & value )
    {
        serialize_bits( stream, value, 16 );
    }

    template <typename Stream> void serialize_int32( Stream & stream, int32_t & value )
    {
        serialize_bits( stream, value, 32 );
    }

    template <typename Stream> void serialize_int64( Stream & stream, int64_t & value )
    {
        uint32_t hi,lo;
        if ( Stream::IsWriting )
        {
            lo = uint64_t(value) & 0xFFFFFFFF;
            hi = uint64_t(value) >> 32;
        }
        serialize_bits( stream, lo, 32 );
        serialize_bits( stream, hi, 32 );
        if ( Stream::IsReading )
            value = ( int64_t(hi) << 32 ) | lo;
    }

    template <typename Stream> inline void internal_serialize_float( Stream & stream, float & value, float min, float max, float res )
    {
        const float delta = max - min;
        const float values = delta / res;
        const uint32_t maxIntegerValue = (uint32_t) ceil( values );
        const int bits = bits_required( 0, maxIntegerValue );
        
        uint32_t integerValue = 0;
        
        if ( Stream::IsWriting )
        {
            float normalizedValue = clamp( ( value - min ) / delta, 0.0f, 1.0f );
            integerValue = (uint32_t) floor( normalizedValue * maxIntegerValue + 0.5f );
        }
        
        stream.SerializeBits( integerValue, bits );

        if ( Stream::IsReading )
        {
            const float normalizedValue = integerValue / float( maxIntegerValue );
            value = normalizedValue * delta + min;
        }
    }

    #define serialize_compressed_float( stream, value, min, max, res )                                        \
    do                                                                                                        \
    {                                                                                                         \
        internal_serialize_float( stream, value, min, max, res );                                             \
    }                                                                                                         \
    while(0)

    template <typename Stream> void serialize_double( Stream & stream, double & value )
    {
        union DoubleInt
        {
            double double_value;
            uint64_t int_value;
        };

        DoubleInt tmp;
        if ( Stream::IsWriting )
            tmp.double_value = value;

        serialize_uint64( stream, tmp.int_value );

        if ( Stream::IsReading )
            value = tmp.double_value;
    }

    template <typename Stream> void serialize_bytes( Stream & stream, uint8_t* data, int bytes )
    {
        stream.SerializeBytes( data, bytes );        
    }

    template <typename Stream> void serialize_string( Stream & stream, char* string, int buffer_size )
    {
        uint32_t length;
        if ( Stream::IsWriting )
            length = strlen( string );
        stream.Align();
        stream.SerializeBits( length, 32 );
        assert( length < buffer_size - 1 );
        stream.SerializeBytes( (uint8_t*)string, length );
        if ( Stream::IsReading )
            string[length] = '\0';
    }

    template <typename Stream> bool serialize_check( Stream & stream, uint32_t magic )
    {
        return stream.SerializeCheck( magic );
    }
    */

    class Object
    {  
    public:

        virtual ~Object() {}

        virtual bool SerializeRead( class ReadStream & stream ) = 0;

        virtual bool SerializeWrite( class WriteStream & stream ) = 0;

        virtual bool SerializeMeasure( class MeasureStream & stream ) = 0;
    };

    #define PROTOCOL2_SERIALIZE_OBJECT( stream )                                                              \
        bool SerializeRead( class protocol2::ReadStream & stream ) { return Serialize( stream ); };           \
        bool SerializeWrite( class protocol2::WriteStream & stream ) { return Serialize( stream ); };         \
        bool SerializeMeasure( class protocol2::MeasureStream & stream ) { return Serialize( stream ); };     \
        template <typename Stream> bool Serialize( Stream & stream )                            

    class Packet : public Object
    {
        int type;

    public:
        
        Packet( int _type ) : type(_type) {}

        int GetType() const { return type; }

    protected:

        virtual ~Packet() {}

        friend class PacketFactory;

    private:

        Packet( const Packet & other );
        Packet & operator = ( const Packet & other );
    };

    // todo: move packet factory into implementation

    class PacketFactory
    {        
#if PROTOCOL2_DEBUG_MEMORY_LEAKS
        std::map<void*,int> allocated_packets;
#endif // #if PROTOCOL2_DEBUG_MEMORY_LEAKS

        int m_numTypes;

        int m_numAllocatedPackets;          // todo: make this dev build only

    public:

        PacketFactory( int numTypes )
        {
            m_numTypes = numTypes;
            m_numAllocatedPackets = 0;
        }

        ~PacketFactory()
        {
#if PROTOCOL2_DEBUG_MEMORY_LEAKS
            if ( allocated_packets.size() )
            {
                printf( "you leaked packets!\n" );
                printf( "%d packets leaked\n", m_numAllocatedPackets );
                for ( auto itor : allocated_packets )
                {
                    auto p = itor.first;
                    printf( "leaked packet %p\n", p );
                }
            }
#endif // #if PROTOCOL2_DEBUG_MEMORY_LEAKS

            assert( m_numAllocatedPackets == 0 );
        }

        Packet* CreatePacket( int type )
        {
            assert( type >= 0 );
            assert( type < m_numTypes );

            Packet * packet = CreateInternal( type );
            
#if PROTOCOL2_DEBUG_MEMORY_LEAKS
            printf( "create packet %p\n", packet );
            allocated_packets[packet] = 1;
            auto itor = allocated_packets.find( packet );
            assert( itor != allocated_packets.end() );
#endif // #if PROTOCOL2_DEBUG_MEMORY_LEAKS
            
            m_numAllocatedPackets++;

            return packet;
        }

        void DestroyPacket( Packet* packet )
        {
            assert( packet );

#if PROTOCOL2_DEBUG_MEMORY_LEAKS
            printf( "destroy packet %p\n", packet );
            auto itor = allocated_packets.find( packet );
            assert( itor != allocated_packets.end() );
            allocated_packets.erase( packet );
#endif // #if PROTOCOL2_DEBUG_MEMORY_LEAKS

            assert( m_numAllocatedPackets > 0 );
            m_numAllocatedPackets--;

            delete packet;
        }

        int GetNumTypes() const
        {
            return m_numTypes;
        }

    protected:

        virtual Packet* CreateInternal( int type ) = 0;
    };

    uint32_t calculate_crc32( const uint8_t *buffer, size_t length, uint32_t crc32 = 0 );

    int write_packet( Packet *packet, const PacketFactory & packetFactory, uint8_t *buffer, int bufferSize, uint32_t protocolId );

    #define PROTOCOL2_READ_PACKET_ERROR_NONE                0
    #define PROTOCOL2_READ_PACKET_ERROR_CRC32               1
    #define PROTOCOL2_READ_PACKET_INVALID_PACKET_TYPE       2
    #define PROTOCOL2_READ_PACKET_FAILED_TO_CREATE_PACKET   3
    #define PROTOCOL2_READ_PACKET_SERIALIZE_PACKET_FAILED   4
    #define PROTOCOL2_READ_PACKET_SERIALIZE_CHECK_FAILED    5

    Packet* read_packet( PacketFactory & packetFactory, const uint8_t *buffer, int bufferSize, uint32_t protocolId, int *error = NULL );
}

#endif // #ifndef PROTOCOL2_H

#ifdef PROTOCOL2_IMPLEMENTATION

namespace protocol2
{
    static const uint32_t crc32_table[256] = 
    {
        0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
        0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
        0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
        0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
        0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
        0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
        0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
        0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
        0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
        0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
        0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
        0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
        0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
        0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
        0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
        0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
        0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
        0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
        0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
        0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
        0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
        0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
        0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
        0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
        0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
        0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
        0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
        0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
        0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
        0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
        0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
        0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D 
    };

    uint32_t calculate_crc32( const uint8_t *buffer, size_t length, uint32_t crc32 )
    {
        crc32 ^= 0xFFFFFFFF;
        for ( size_t i = 0; i < length; ++i ) 
            crc32 = ( crc32 >> 8 ) ^ crc32_table[ ( crc32 ^ buffer[i] ) & 0xFF ];
        return crc32 ^ 0xFFFFFFFF;
    }

    inline int write_packet( Packet *packet, const PacketFactory & packetFactory, uint8_t *buffer, int bufferSize, uint32_t protocolId )
    {
        assert( packet );
        assert( buffer );
        assert( bufferSize > 0 );
        assert( protocolId != 0 );

        typedef WriteStream Stream;

        Stream stream( buffer, bufferSize );

        uint32_t crc32 = 0;
        stream.SerializeBits( crc32, 32 );

        int packetType = packet->GetType();

        stream.SerializeInteger( packetType, 0, packetFactory.GetNumTypes() );

        packet->SerializeWrite( stream );

        stream.SerializeCheck( protocolId );

        stream.Flush();

        crc32 = calculate_crc32( buffer, stream.GetBytesProcessed() );

        *((uint32_t*)buffer) = host_to_network( crc32 );

        assert( !stream.GetError() );

        if ( stream.GetError() )
            return 0;

        return stream.GetBytesProcessed();
    }

    inline Packet* read_packet( PacketFactory & packetFactory, const uint8_t *buffer, int bufferSize, uint32_t protocolId, int *error )
    {
        assert( buffer );
        assert( bufferSize > 0 );
        assert( protocolId != 0 );

        const int paddedSize = 4 * ( bufferSize / 4 ) + ( ( bufferSize % 4 ) ? 4 : 0 );

        assert( paddedSize >= bufferSize );

        typedef protocol2::ReadStream Stream;

        Stream stream( buffer, paddedSize );

        uint32_t read_crc32;
        stream.SerializeBits( read_crc32, 32 );

        *((uint32_t*)buffer) = 0;

        const uint32_t crc32 = calculate_crc32( buffer, bufferSize );

        if ( crc32 != read_crc32 )
        {
            if ( error )
                *error = PROTOCOL2_READ_PACKET_ERROR_CRC32;
            return NULL;
        }

        int packetType;

        if ( !stream.SerializeInteger( packetType, 0, packetFactory.GetNumTypes() ) )
        {
            if ( error )
                *error = PROTOCOL2_READ_PACKET_INVALID_PACKET_TYPE;
            return NULL;
        }

        protocol2::Packet *packet = packetFactory.CreatePacket( packetType );
        if ( !packet )
        {
            if ( error )
                *error = PROTOCOL2_READ_PACKET_FAILED_TO_CREATE_PACKET;
            return NULL;
        }

        if ( !packet->SerializeRead( stream ) )
        {
            if ( error )
                *error = PROTOCOL2_READ_PACKET_SERIALIZE_PACKET_FAILED;
            goto failure;
        }

        if ( !stream.SerializeCheck( protocolId ) )
        {
            if ( error )
                *error = PROTOCOL2_READ_PACKET_SERIALIZE_CHECK_FAILED;
            goto failure;
        }

        return packet;

    failure:
        packetFactory.DestroyPacket( packet );
        return NULL;
    }
}

#endif // #ifdef PROTOCOL2_IMPLEMENTATION
