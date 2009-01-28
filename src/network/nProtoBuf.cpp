/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2005  by 
and the AA DevTeam (see the file AUTHORS(.txt) in the main source directory)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
  
***************************************************************************

*/

#include "nProtoBuf.h"
#include "tConsole.h"
#include "nNetObject.h"
#include "nBinary.h"

#include "google/protobuf/message.h"
#include "google/protobuf/descriptor.h"

#include "nNetObject.pb.h"

using namespace google::protobuf;

#ifdef DEBUG
// print debug strings?
// #define DEBUG_STRINGS
#endif

#if GOOGLE_PROTOBUF_VERSION < 2000003
// cull first parameter from reflection functions
#define REFL_GET( function, message, field )        function( field )
#define REFL_SET( function, message, field, value ) function( field, value )
typedef nProtoBuf::Reflection Reflection;
#define REFLECTION_CONST Reflection
#else
#define REFL_GET( function, message, field )        function( message, field )
#define REFL_SET( function, message, field, value ) function( message, field, value )
#define REFLECTION_CONST Reflection const
#endif

//! fills the receiving buffer with data
int nMessageFillerProtoBufBase::OnFill( FillArguments & arguments ) const
{
    nProtoBuf const & fullProtoBuf = GetProtoBuf();

#ifdef DEBUG
    fullProtoBuf.CheckInitialized();
#endif

    unsigned short descriptor = arguments.message_.Descriptor();

    // does the receiver understand ProtoBufs?a
    if ( sn_protoBuf.Supported( arguments.receiver_ ) )
    {
        // yep. Stream them.

        // compress message, writing the cache ID
        nProtoBuf & workProtoBuf = AccessWorkProtoBuf();
        nMessageCache & cache = sn_Connections[ arguments.receiver_ ].messageCacheOut_;
        arguments.Write( cache.CompressProtoBuff( fullProtoBuf, workProtoBuf ) );

        // dump into plain stringstream
        std::stringstream rw;
        workProtoBuf.SerializeToOstream( &rw );

        // write stringstream to message
        while( !rw.eof() )
        {
            unsigned short value = ((unsigned char)rw.get()) << 8;
            if ( !rw.eof() )
            {
                value += (unsigned char)rw.get();
            }
            arguments.Write( value );
        }
    }
    else
    {
        // receiver does not understant ProtoBufs. Transform
        // to old style message.
        static nDescriptor dummy( 0, NULL, "dummy" );
        if ( !oldFormat_ )
        {
            oldFormat_ = new nMessage( dummy );
            nProtoBufDescriptorBase::StreamToStatic( fullProtoBuf, *oldFormat_ );
        }

        // and copy the message contents over.
        for( int i = 0; i < oldFormat_->DataLen(); ++i )
        {
            arguments.Write( oldFormat_->Data(i) );
        }

        // bend the descriptor
        descriptor &= ~nProtoBufDescriptorBase::protoBufFlag;
    }

    return descriptor;
}

/*
Not defined here, but in nNetwork.cpp for access to the right static variables and macros.

nProtoBufDescriptorBase::nProtoBufDescriptorBase(unsigned short identification,
                                     const char * name, 
                                     bool acceptEvenIfNotLoggedIn )
*/

std::string const & nProtoBufDescriptorBase::DetermineName( nProtoBuf const & prototype )
{
    return prototype.GetDescriptor()->full_name();
}

nProtoBufDescriptorBase::DescriptorMap const & nProtoBufDescriptorBase::GetDescriptorsByName()
{
    return descriptorsByName;
}

//! puts a filler into a message, taking ownership of it
nMessage * nProtoBufDescriptorBase::Transform( nMessageFillerProtoBufBase * filler ) const
{
    nMessage * ret = new nMessage( *this );
    
    ret->SetFiller( filler );
    
    return ret;
}

nProtoBufDescriptorBase::DescriptorMap nProtoBufDescriptorBase::descriptorsByName;
//! dumb streaming from message, static version
void nProtoBufDescriptorBase::StreamFromStatic( nMessage & in, nProtoBuf & out  )
{
    // try to determine suitable descriptor
    nProtoBufDescriptorBase const * embedded = descriptorsByName[ DetermineName( out ) ];
    if ( embedded )
    {
        // found it, delegate
        embedded->StreamFrom( in, out );
    }
    else
    {
#ifdef DEBUG_X
        static bool warn = true;
        if ( warn )
        {
            tERR_WARN( "Unknown buffer format " << DetermineName( out ) << ", not sure if it'll get streamed correctly." );
            warn = false;
        }
#endif
        
        // fallback to default
        StreamFromDefault( in, out );
    }
}

//! dumb streaming to message, static version
void nProtoBufDescriptorBase::StreamToStatic( nProtoBuf const & in, nMessage & out )
{
    // try to determine suitable descriptor
    nProtoBufDescriptorBase const * embedded = descriptorsByName[ DetermineName( in ) ];
    if ( embedded )
    {
        // found it, delegate
        embedded->StreamTo( in, out );
    }
    else
    {
#ifdef DEBUG_X
        static bool warn = true;
        if ( warn )
        {
            tERR_WARN( "Unknown buffer format, not sure if it'll get streamed correctly." );
            warn = false;
        }
#endif
        
        // fallback to default
        StreamToDefault( in, out );
    }
}

//! dumb streaming from message, static version
void nProtoBufDescriptorBase::StreamFromDefault( nMessage & in, nProtoBuf & out  )
{
    // get reflection interface
    REFLECTION_CONST * reflection = out.GetReflection();
    Descriptor const * descriptor = out.GetDescriptor();

    // iterate over fields in ID order
    int count = descriptor->field_count();
    for( int i = 0; i < count; ++i )
    {
        FieldDescriptor const * field = descriptor->field( i );
        tASSERT( field );

        if ( in.End() )
        {
            break;
        }

        if ( FieldDescriptor::kLastReservedNumber < field->number() )
        {
            // end marker
            break;
        }

        switch( field->cpp_type() )
        {
        case FieldDescriptor::CPPTYPE_INT32:
        {
            int32 value;
            in >> value;
            reflection->REFL_SET( SetInt32, &out, field, value );
        }
        break;
        case FieldDescriptor::CPPTYPE_UINT32:
        {
            unsigned short value;
            in.Read( value );
            reflection->REFL_SET( SetUInt32, &out, field, value );
        }
        break;
        case FieldDescriptor::CPPTYPE_STRING:
        {
            tString value;
            in >> value;
            reflection->REFL_SET( SetString, &out, field, value );
        }
        break;
        case FieldDescriptor::CPPTYPE_FLOAT:
        {
            REAL value;
            in >> value;
            reflection->REFL_SET( SetFloat, &out, field, value );
        }
        break;
        case FieldDescriptor::CPPTYPE_BOOL:
        {
            bool value;
            in >> value;
            reflection->REFL_SET( SetBool, &out, field, value );
        }
        break;
        case FieldDescriptor::CPPTYPE_MESSAGE:
            StreamFromStatic( in, *reflection->REFL_GET( MutableMessage, &out, field ) );
            break;

        // explicitly unsupported:
        case FieldDescriptor::CPPTYPE_ENUM:
        case FieldDescriptor::CPPTYPE_INT64:
        case FieldDescriptor::CPPTYPE_UINT64:
        case FieldDescriptor::CPPTYPE_DOUBLE:
        default:
            tERR_ERROR( "Unsupported field type." );
            break;
        }
    }
}

//! dumb streaming to message, static version
void nProtoBufDescriptorBase::StreamToDefault( nProtoBuf const & in, nMessage & out )
{
    // get reflection interface
    const Reflection * reflection = in.GetReflection();
    const Descriptor * descriptor = in.GetDescriptor();

    // iterate over fields in ID order
    int count = descriptor->field_count();
    for( int i = 0; i < count; ++i )
    {
        FieldDescriptor const * field = descriptor->field( i );

        if ( FieldDescriptor::kLastReservedNumber < field->number() )
        {
            // end marker
            break;
        }

        switch( field->cpp_type() )
        {
        case FieldDescriptor::CPPTYPE_INT32:
            out << reflection->REFL_GET( GetInt32, in, field );
            break;
        case FieldDescriptor::CPPTYPE_UINT32:
            out.Write( reflection->REFL_GET( GetUInt32, in, field ) );
            break;
        case FieldDescriptor::CPPTYPE_STRING:
            out << reflection->REFL_GET( GetString, in, field );
            break;
        case FieldDescriptor::CPPTYPE_FLOAT:
            out << reflection->REFL_GET( GetFloat, in, field );
            break;
        case FieldDescriptor::CPPTYPE_BOOL:
            out << reflection->REFL_GET( GetBool, in, field );
            break;
        case FieldDescriptor::CPPTYPE_MESSAGE:
            StreamToStatic( reflection->REFL_GET( GetMessage, in, field ), out );
            break;

        // explicitly unsupported:
        case FieldDescriptor::CPPTYPE_ENUM:
        case FieldDescriptor::CPPTYPE_INT64:
        case FieldDescriptor::CPPTYPE_UINT64:
        case FieldDescriptor::CPPTYPE_DOUBLE:
        default:
            tERR_ERROR( "Unsupported field type." );
            break;
        }
    }
}   

//! selective reading message, either embedded or transformed
void nProtoBufDescriptorBase::ReadMessage( nMessageBase & in2, nProtoBuf & out, nProtoBuf & work ) const
{
    nStreamMessage & in = dynamic_cast< nStreamMessage & >( in2 );

    if ( in.descriptor & protoBufFlag )
    {
        // message is a proper protocol buffer message.

        // read the cache ID
        unsigned short cacheID;
        in.Read( cacheID );

        // pre-fill
        nMessageCache & cache = sn_Connections[ in.SenderID() ].messageCacheIn_;
        bool inCache = cache.UncompressProtoBuf( cacheID, out );

        // fill rest of fields

        // first, read into a string. protobufs ALWAYS take the whole rest of a
        // message. Luckily, they don't mind a single appended 0 byte.
        std::stringstream rw;
        while( !in.End() )
        {
            unsigned short value;
            in.Read( value );
            rw.put( value >> 8 );
            rw.put( value & 0xff );
        }

        // then, read the string into the buffer
        if ( inCache )
        {
            work.ParsePartialFromIstream( &rw );

#ifdef DEBUG_STRINGS
            con << "Base  : " << out.ShortDebugString() << "\n";
            con << "Diff  : " << work.ShortDebugString() << "\n";
#endif

            out.MergeFrom( work );

#ifdef DEBUG_STRINGS
            con << "Merged: " << out.ShortDebugString() << "\n";
#endif
        }
        else
        {
            // just read directly
            out.ParsePartialFromIstream( &rw );
        }

        // TODO: optimize. The stream copy should not be required, the data is
        // already in memory. But first, make it so that the byte order in memory
        // is already the network byte order, right now, it's the machine's order.
    }
    else
    {
        // transform from old format
        StreamFrom( in, out );
    }
}

//! compares two messages, filling in total size and difference.
//! @param a first message
//! @param b second message
//! @param total total maximal size of message (must be zeroed before call)
//! @param total difference of messages, between 0 and total (must be zeroed before call)
//! @param removed is set to true if a has an element that is missing from b
void nProtoBufDescriptorBase::EstimateMessageDifference( nProtoBuf const & a,
                                                   nProtoBuf const & b,
                                                   int & total,
                                                   int & difference,
                                                   bool & removed )
{
    // get reflection interface
    const Reflection * ra = a.GetReflection();
    Descriptor const * descriptor  = a.GetDescriptor();
    const Reflection * rb = b.GetReflection();
    tASSERT( descriptor == b.GetDescriptor() );

#ifdef DEBUG
    // tString da = a.ShortDebugString();
    // tString db = b.ShortDebugString();
#endif    

    // iterate over fields in ID order
    int count = descriptor->field_count();
    for( int i = 0; i < count; ++i )
    {
        FieldDescriptor const * field = descriptor->field( i );
        tASSERT( field );

        int weight = 0;
        bool differ = false;

        if ( field->label() == FieldDescriptor::LABEL_REPEATED )
        {
            continue;
        }

        switch( field->cpp_type() )
        {
#define COMPARE( getter, w ) weight = w; differ = ( ra->REFL_GET( getter, a, field ) != rb->REFL_GET( getter, b, field ) )
        case FieldDescriptor::CPPTYPE_INT32:
            COMPARE( GetInt32, 2 );
            break;
        case FieldDescriptor::CPPTYPE_UINT32:
            COMPARE( GetUInt32, 2 );
            break;
        case FieldDescriptor::CPPTYPE_STRING:
            COMPARE( GetString, 8 );
            break;
        case FieldDescriptor::CPPTYPE_FLOAT:
            COMPARE( GetFloat, 4 );
            break;
        case FieldDescriptor::CPPTYPE_BOOL:
            COMPARE( GetBool, 1 );
            break;
        case FieldDescriptor::CPPTYPE_ENUM:
            COMPARE( GetEnum, 2 );
            break;
    case FieldDescriptor::CPPTYPE_INT64:
            COMPARE( GetInt64, 4 );
            break;
        case FieldDescriptor::CPPTYPE_UINT64:
            COMPARE( GetUInt64, 4 );
            break;
        case FieldDescriptor::CPPTYPE_DOUBLE:
            COMPARE( GetDouble, 8 );
            break;
        case FieldDescriptor::CPPTYPE_MESSAGE:
            EstimateMessageDifference( ra->REFL_GET( GetMessage, a, field ), rb->REFL_GET( GetMessage, b, field ), total, difference, removed );
            break;
        default:
            tERR_ERROR( "Unsupported field type." );
            break;
#undef COMPARE
        }
    
        // accounting
        total += weight;
        if ( differ || ra->REFL_GET( HasField, a, field ) != rb->REFL_GET( HasField, b, field ) )
        {
            difference += weight;
        }

        // check for missing fields
        if ( ra->REFL_GET( HasField, a, field ) && ! rb->REFL_GET( HasField, b, field ) )
        {
            removed = true;
        }
    }
}

//! calculates the difference between two messages
//! @param base base message
//! @param derived second message
//! @param diff target message for difference. All the fields where base and derived differ are set here, with the value taken from derived.
//! @param copy if true, the first operation is a copy from derived to diff. If false, only elements equal in both derived and base are discarded from diff.
void nProtoBufDescriptorBase::DiffMessages( nProtoBuf const & base,
                                      nProtoBuf const & derived,
                                      nProtoBuf & diff,
                                      bool copy )
{
    // get reflection interface
    Reflection const * r_base     = base.GetReflection();
    Descriptor const * descriptor = base.GetDescriptor();
    Reflection const * r_derived  = derived.GetReflection();
    REFLECTION_CONST *  r_diff     = diff.GetReflection();
    tASSERT( descriptor == derived.GetDescriptor() );
    tASSERT( descriptor == diff.GetDescriptor() );

    // make copy
    if ( copy )
    {
        diff.CopyFrom( derived );
    }

    // iterate over fields in ID order
    int count = descriptor->field_count();
    for( int i = 0; i < count; ++i )
    {
        FieldDescriptor const * field = descriptor->field( i );
        tASSERT( field );

        if ( field->label() == FieldDescriptor::LABEL_REPEATED )
        {
            continue;
        }

        if ( !r_base->REFL_GET( HasField, base, field ) )
        {
            continue;
        }
        
        bool differ = false;
        switch( field->cpp_type() )
        {
#define COMPARE( getter ) differ = ( r_base->REFL_GET( getter, base, field ) != r_derived->REFL_GET( getter, derived, field ) )
        case FieldDescriptor::CPPTYPE_INT32:
            COMPARE( GetInt32 );
            break;
        case FieldDescriptor::CPPTYPE_UINT32:
            COMPARE( GetUInt32 );
            break;
        case FieldDescriptor::CPPTYPE_STRING:
            COMPARE( GetString );
            break;
        case FieldDescriptor::CPPTYPE_FLOAT:
            COMPARE( GetFloat );
            break;
        case FieldDescriptor::CPPTYPE_BOOL:
            COMPARE( GetBool );
            break;
        case FieldDescriptor::CPPTYPE_ENUM:
            COMPARE( GetEnum );
            break;
    case FieldDescriptor::CPPTYPE_INT64:
            COMPARE( GetInt64 );
            break;
        case FieldDescriptor::CPPTYPE_UINT64:
            COMPARE( GetUInt64 );
            break;
        case FieldDescriptor::CPPTYPE_DOUBLE:
            COMPARE( GetDouble );
            break;
        case FieldDescriptor::CPPTYPE_MESSAGE:
            DiffMessages( r_base->REFL_GET( GetMessage, base, field ), r_derived->REFL_GET( GetMessage, derived, field ), *r_diff->REFL_GET( MutableMessage, &diff, field ), false );
            break;
        default:
            tERR_ERROR( "Unsupported field type." );
            break;
#undef COMPARE
        }
    
        if ( !differ )
        {
            // clear the field
            r_diff->REFL_GET( ClearField, &diff, field );
        }
    }
}

//! @param message the message to rid of all repeated fields
void nProtoBufDescriptorBase::ClearRepeated( nProtoBuf & message )
{
    // get reflection interface
    Descriptor const * descriptor = message.GetDescriptor();
    REFLECTION_CONST * reflection = message.GetReflection();

    // iterate over fields in ID order
    int count = descriptor->field_count();
    for( int i = 0; i < count; ++i )
    {
        FieldDescriptor const * field = descriptor->field( i );
        tASSERT( field );

        if ( field->label() == FieldDescriptor::LABEL_REPEATED )
        {
            // clear the field
            reflection->REFL_GET( ClearField, &message, field );
            continue;
        }

        switch( field->cpp_type() )
        {
        case FieldDescriptor::CPPTYPE_MESSAGE:
            ClearRepeated( *reflection->REFL_GET( MutableMessage, &message, field ) );
            break;
        default:
            break;
        }
    }
}

//! dumb streaming to message
void nProtoBufDescriptorBase::DoStreamTo( nProtoBuf const & in, nMessage & out ) const
{
    StreamToDefault( in, out );
}

//! dumb streaming from message
void nProtoBufDescriptorBase::DoStreamFrom( nMessage & in, nProtoBuf & out ) const
{
    StreamFromDefault( in, out );
}

// read/write operators for protocol buffers
nMessage& operator >> ( nMessage& m, nProtoBuf & buffer )
{

    return m;
}

nMessage& operator << ( nMessage& m, nProtoBuf const & buffer )
{
    tASSERT(0);

    // dump into plain stringstream
    std::stringstream rw;
    buffer.SerializeToOstream( &rw );

    // write stringstream to message
    while( !rw.eof() )
    {
        unsigned short value = ((unsigned char)rw.get()) << 8;
        if ( !rw.eof() )
        {
            value += (unsigned char)rw.get();
        }
        m.Write( value );
    }

    // mark message as final
    m.Finalize();
   
    return m;
}

nOProtoBufDescriptorBase::~nOProtoBufDescriptorBase()
{}

unsigned int nOProtoBufDescriptorBase::GetObjectID ( Network::nNetObjectInit const & message )
{
    return message.object_id();
}

//! checks to run before creating a new object
//! @return true if all is OK
bool nOProtoBufDescriptorBase::PreCheck( unsigned short id, nSenderInfo sender )
{
    if (sn_netObjectsOwner[id]!=sender.SenderID() || bool(sn_netObjects[id]))
    {
#ifdef DEBUG
        st_Breakpoint();
        if (!sn_netObjects[id])
        {
            con << "Netobject " << id << " is already reserved!\n";
        }
        else
        {
            con << "Netobject " << id << " is already defined!\n";
        }
#endif
        if (sn_netObjectsOwner[id]!=sender.SenderID())
        {
            Cheater( sender.SenderID() );
            nReadError();
        }

        return false;
    }

    // all OK
    return true;
}

//! checks to run after creating a new object
void nOProtoBufDescriptorBase::PostCheck( nNetObject * object, nSenderInfo sender )
{
#ifdef DEBUG
    /*
      tString str;
      n->PrintName( str );
      con << "Received object " << str << "\n";
    */
#endif
            
    if ( sn_GetNetState()==nSERVER && !object->AcceptClientSync() )
    {
        object->Release();
        Cheater( sender.SenderID() ); // cheater!
    }
    else if ( static_cast< nNetObject* >( sn_netObjects[ object->ID() ] ) != object )
    {
        // object was unable to be registered
        object->Release(); // silently delete it.
    }
}

//! clears the cache
void nMessageCache::Clear()
{
    parts.clear();
}

static int sn_messageCacheSize = 1000;

//! adds a message to the cache
//! @param message the message to add
//! @param incoming true for incoming messages, keep an extra long backlog then
//! @param reallyAdd true if message should be added. Otherwise, we just purge old messages.
void nMessageCache::AddMessage( nMessageBase * message, bool incoming, bool reallyAdd )
{
    // fetch message filler, ignoring invalid ones
    if ( !message || message->MessageID() == 0 )
    {
        return;
    }

    nMessageFillerProtoBufBase * filler = dynamic_cast< nMessageFillerProtoBufBase * >( message->GetFiller() );

    if ( !filler )
    {
        return;
    }

#ifdef DEBUG_STRINGS
    if ( !reallyAdd )
    {
        con << "NOT ";
    }
    con << "Adding message " << message->MessageID() << " to cache.\n";
#endif    

    // get the cache belonging to this descriptor
    Descriptor const * descriptor = filler->GetProtoBuf().GetDescriptor();
    nMessageCacheByDescriptor & cache = parts[ descriptor ];

    // and push the message into the cache
    if ( reallyAdd )
    {
        cache.queue_.push_back( message );
    }

    // purge old messages
    const int backlog = sn_messageCacheSize;
    int back = backlog;
    if ( incoming )
    {
        back = 100 + back * 2;
    }
    int max = 0xffff;
    if ( !incoming )
    {
        max /= 2;
    }
    if ( back > max )
    {
        back = max;
    }

    if ( !incoming )
    {
        int x;
        x = 0;
    }

    while ( cache.queue_.size() > 0 && cache.queue_.front()->MessageID() + back < message->MessageID() )
    {
        cache.queue_.pop_front();
    }
}

//! fill protobuf from cache
bool nMessageCache::UncompressProtoBuf( unsigned short cacheID, nProtoBuf & target )
{
    // clear target
    target.Clear();

    // do nothing if told so
    if ( cacheID == 0 )
    {
        return false;
    }

    // get the cache belonging to this descriptor
    Descriptor const * descriptor = target.GetDescriptor();
    nMessageCacheByDescriptor & cache = parts[ descriptor ];

    // find the cacheID
    typedef nMessageCacheByDescriptor::CacheQueue CacheQueue;
    for( CacheQueue::reverse_iterator i = cache.queue_.rbegin();
         i != cache.queue_.rend(); ++i )
    {
        nMessageBase * message = *i;
        if ( ( message->MessageID() & 0xffff ) == cacheID )
        {
            // found it, copy over
            tASSERT( dynamic_cast< nMessageFillerProtoBufBase * >( message->GetFiller() ) );
            nMessageFillerProtoBufBase * filler = static_cast< nMessageFillerProtoBufBase * >( message->GetFiller() );
            
            target.CopyFrom( filler->GetProtoBuf() );
            
            nProtoBufDescriptorBase::ClearRepeated( target );

            return true;
        }
    }

    tASSERT( 0 );

    throw nKillHim();

    return false;
}

//! find suitable previous message and compresses
//! the passed protobuf. Return value: the cache ID.
unsigned short nMessageCache::CompressProtoBuff( nProtoBuf const & source, nProtoBuf & target )
{
    // get the cache belonging to this descriptor
    Descriptor const * descriptor = target.GetDescriptor();
    nMessageCacheByDescriptor & cache = parts[ descriptor ];

    // try to find the best matching message in the queue
    nMessageBase * best = 0;

    // total size, maximum size to save
    int size = 0;
    
    // best difference achieved so far
    int bestDifference = 0;
    
    // attempts so far
    int count = 0;

    // count when last best message was found
    int bestCount = 0;

    // find the cacheID
    typedef nMessageCacheByDescriptor::CacheQueue CacheQueue;
    for( CacheQueue::reverse_iterator i = cache.queue_.rbegin();
         i != cache.queue_.rend(); ++i )
    {
        nMessageBase * message = *i;

        tASSERT( dynamic_cast< nMessageFillerProtoBufBase * >( message->GetFiller() ) );
        nMessageFillerProtoBufBase * filler = static_cast< nMessageFillerProtoBufBase * >( message->GetFiller() );
        nProtoBuf const & cached = filler->GetProtoBuf();

        // the current difference
        int difference = 0;
        size = 0;

        // whether there were removed fields
        bool removed = false;

        nProtoBufDescriptorBase::
        EstimateMessageDifference( cached, source,
                                   size, difference, removed );
        if ( !removed && ( !best || difference < bestDifference ) )
        {
            // best message so far
            best = message;
            bestDifference = difference;
        }

        // check whether it pays to look on
        if ( bestDifference * count > 2 * bestCount * size )
        {
            break;
        }

        ++count;
    }
    
    if ( best )
    {
        // found a good previous message. yay!
        tASSERT( dynamic_cast< nMessageFillerProtoBufBase * >( best->GetFiller() ) );
        nMessageFillerProtoBufBase * filler = static_cast< nMessageFillerProtoBufBase * >( best->GetFiller() );
        nProtoBuf const & cached = filler->GetProtoBuf();

        // calculate diff message
        nProtoBufDescriptorBase::
        DiffMessages( cached, source, target );

#ifdef DEBUG_STRINGS
        con << "Size = " << size << ", diff = " << bestDifference << "\n";
        con << "Base  : " << cached.ShortDebugString() << "\n";
        con << "Source: " << source.ShortDebugString() << "\n";
        con << "Diff  : " << target.ShortDebugString() << "\n";
#endif

        // and return the message ID
        return static_cast< unsigned short >( best->MessageID() );
    }
    else
    {
        // no message found, return full protobuf
        target.CopyFrom( source );
        return 0;
    }
}

#include "nConfig.h"

static nSettingItem< int > sn_messageCacheSizeConf( "MESSAGE_CACHE_SIZE", sn_messageCacheSize );

