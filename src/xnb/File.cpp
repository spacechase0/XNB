#include "xnb/File.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <SFML/Config.hpp>
#include <util/Logger.hpp>
#include <util/String.hpp>

#include "xnb/ITypeReader.hpp"
#include "xnb/Util.hpp"

#include "Lzx.hpp"

namespace
{
    std::string decompress( std::istream& in, std::size_t compressed, std::size_t decompressed )
    {
        std::stringstream out;
        
        in.exceptions( std::istream::goodbit );
        
        try
        {
            lzx::LzxDecoder dec( 16 );
            int x = in.tellg();
            while ( in.tellg() - 14 < compressed )
            {
                in.seekg( x );
                //std::cout << "presize:" << in.tellg() << std::endl;
                
                sf::Uint8 a = in.get(), b = in.get();
                x += 2;
                //std::cout<<"b:"<<(int)a<<' '<<(int)b<<std::endl;
                int chunk = 0x8000;
                int block = ( a << 8 ) | b;
                if ( a == 0xFF )
                {
                    chunk = ( b << 8 ) | in.get();
                    block = ( in.get() << 8 ) | in.get();
                    x += 3;
                }
                
                
                if ( chunk == 0 || block == 0 )
                    break;
                
                //std::cout<<"C&B "<<chunk<<' '<<block<<std::endl;
                int err = dec.Decompress( in, block, out, chunk );
                if ( err != 0 )
                    break;
                x += block;
                //std::cout << "Finished: " << err << ' ' << stream->error << std::endl;
            }
        }
        catch ( std::exception& e )
        {
            throw e;
        }
        
        std::string ret = out.str();
        //std::cout << in.tellg() << ' ' << compressed << " decompressed: " << ret.length() << ' ' << ret << std::endl;
        
        return out.str();
    }
    /*
    std::string compress( const std::string& str )
    {
        struct LzxComp
        {
            LzxComp()
            {
                if ( lzx_create_compressor( 0x8000, 32, false, &data ) )
                    throw std::logic_error( "Failed to create compressor" );
            }
            
            ~LzxComp()
            {
                lzx_free_compressor(data);
            }
            void* data = nullptr;
        };
        
        LzxComp comp;
        
        std::ostringstream out;
        int rounds = str.length() / 0x8000 + 1;
        if ( str.length() % 0x8000 == 0 )
            --rounds;
        for ( int i = 0; i < rounds; ++i )
        {
            std::string data( 0x8000, '\0' );
            std::size_t roundSize = i + 1 == rounds ? str.length() % 0x8000 : 0x8000;
            std::size_t ret = lzx_compress( &str[ i * 0x8000 ], roundSize, &data[ 0 ], data.length(), comp.data );
            if ( ret == 0 )
                throw std::invalid_argument( "Too small to compress I guess" );
            
            if ( roundSize != 0x8000 )
            {
                sf::Uint8 specialMarker = 0xFF;
                sf::Uint8 chunkHi = ( roundSize >> 8 ) & 0xFF;
                sf::Uint8 chunkLo = roundSize & 0xFF;
                out.write( reinterpret_cast< char* >( &specialMarker ), 1 );
                out.write( reinterpret_cast< char* >( &chunkHi ), 1 );
                out.write( reinterpret_cast< char* >( &chunkLo ), 1 );
            }
            sf::Uint8 blockHi = ( ret >> 8 ) & 0xFF;
            sf::Uint8 blockLo = ret & 0xFF;
            out.write( reinterpret_cast< char* >( &blockHi ), 1 );
            out.write( reinterpret_cast< char* >( &blockLo ), 1 );
            
            out.write( &data[ 0 ], ret );
        }
        
        return out.str();
    }*/
}

namespace xnb
{
    // TODO: Move
    enum TargetPlatform
    {
        Windows = 'w',
        Phone = 'm',
        Xbox = 'x',
    };
    enum Flags
    {
        HiDef = 0x01,
        Compressed = 0x80,
    };
    
    template< typename T >
    T read( std::istream& in )
    {
        T t;
        in.read( reinterpret_cast< char* >( &t ), sizeof( T ) );
        return t;
    }
    
    /// NOTE: UTF-8. Change to sf::String?
    template<>
    std::string read< std::string >( std::istream& in )
    {
        auto bytes = read7BitEncodedInt( in );
        std::string str( bytes, 0 );
        in.read( &str[ 0 ], bytes );
        return str;
    }
    
    template<>
    TypeReaderHeader read< TypeReaderHeader >( std::istream& in )
    {
        TypeReaderHeader tr;
        tr.name = read< std::string >( in );
        tr.version = read< sf::Int32 >( in );
        return tr;
    }
    
    template< typename T >
    void write( std::ostream& out, const T& t )
    {
        out.write( reinterpret_cast< const char* >( &t ), sizeof( T ) );
    }
    
    /// NOTE: UTF-8. Change to sf::String?
    template<>
    void write< std::string >( std::ostream& out, const std::string& str )
    {
        write7BitEncodedInt( out, str.length() );
        out.write( &str[ 0 ], str.length() );
    }
    
    template<>
    void write< TypeReaderHeader >( std::ostream& out, const TypeReaderHeader& tr )
    {
        write( out, tr.name );
        write( out, tr.version );
    }
    
    bool File::loadFromFile( const std::string& path )
    {
        std::ifstream file( path, std::ifstream::binary );
        if ( !file )
        {
            util::log( "[ERROR] $: Failed to open file.\n", path );
            return false;
        }
        
        util::log( "[INFO] Reading $...\n", path );
        return loadFromStream( file );
    }
    
    bool File::loadFromStream( std::istream& in )
    {
        in.exceptions( std::ifstream::failbit );
        
        try
        {
            if ( in.get() != 'X' || in.get() != 'N' || in.get() != 'B' )
            {
                util::log( "[ERROR] File is not an XNB file.\n" );
                return false;
            }
            if ( in.get() != TargetPlatform::Windows )
            {
                util::log( "[WARN] Other target platforms untested.\n" );
            }
            if ( in.get() != 5 )
            {
                util::log( "[ERROR] Format version unsupported.\n" );
                return false;
            }
            
            char flags = in.get();
            bool hidef = flags & HiDef;
            bool compressed = flags & Compressed;
            
            auto sizeCompressed = read< sf::Uint32 >( in );
            auto sizeDecompressed = compressed ? read< sf::Uint32 >( in ) : sizeCompressed;
            
            if ( compressed )
            {
                std::istringstream ins( decompress( in, sizeCompressed - 14, sizeDecompressed ) );
                ins.exceptions( std::ifstream::failbit );
                if ( readData( ins ) )
                    this->HiDef = hidef;
                else return false;
            }
            else
            {
                if ( readData( in ) )
                    this->HiDef = hidef;
                else return false;
            }
            
            return true;
        }
        catch ( std::exception& e )
        {
            util::log( "[ERROR] Exception reading: $\n", e.what() );
            return false;
        }
    }
    
    bool File::writeToFile( const std::string& path )
    {
        std::ofstream file( path, std::ofstream::binary | std::ofstream::trunc );
        if ( !file )
        {
            util::log( "[ERROR] $: Failed to open file.\n", path );
            return false;
        }
        
        util::log( "[INFO] Writing to $...\n", path );
        return writeToStream( file );
    }
    
    bool File::writeToStream( std::ostream& out )
    {
        out.exceptions( std::ifstream::failbit );
        
        try
        {
            char platform = TargetPlatform::Windows;
            char formatVer = 5;
            char flags = 0;
            if ( this->HiDef )
                flags |= HiDef;
            
            out.write( "XNB", 3 );
            out.write( &platform, 1 );
            out.write( &formatVer, 1 );
            
            std::ostringstream dataOut;
            writeData( dataOut );
            std::string dataStr = dataOut.str();
            sf::Uint32 size = dataStr.length();
            
            std::string compressed;
            /*try
            {
                compressed = compress( dataStr );
                flags |= Compressed;
            }
            catch ( std::exception& e )
            {
                util::log( "[WARN] Exception compressing: $\n", e.what() );
            }*/
            out.write( &flags, 1 );
            if ( compressed != "" )
            {
                write< sf::Uint32 >( out, compressed.size() );
                out.write( reinterpret_cast< char* >( &size ), 4 );
                out.write( compressed.c_str(), compressed.length() );
            }
            else
            {
                out.write( reinterpret_cast< char* >( &size ), 4 );
                out.write( dataStr.c_str(), dataStr.length() );
            }
            
            return ( out );
        }
        catch ( std::exception& e )
        {
            util::log( "[ERROR] Exception writing: $\n", e.what() );
            return false;
        }
    }
    
    bool File::readData( std::istream& in )
    {
        typeReaders.resize( read7BitEncodedInt( in ) );
        for ( TypeReaderHeader& tr : typeReaders )
        {
            tr = read< TypeReaderHeader >( in );
        }
        
        int sharedResCount = read7BitEncodedInt( in );
        
        int type = read7BitEncodedInt( in );
        if ( type == 0 )
        {
            throw std::invalid_argument( util::format( "Null not supported" ) );
        }
        else
        {
            type -= 1;
            std::string typeStr = typeReaders[ type ].name;
            
            ITypeReader* reader = ITypeReader::getTypeReader( typeStr );
            if ( reader == nullptr )
                throw std::invalid_argument( util::format( "Type not supported: $", typeStr ) );
            
            data = reader->read( ( * this ), in, typeStr );
        }
        
        return true;
    }
    
    bool File::writeData( std::ostream& out )
    {
        typeReaders = data->getTypeReaders();
        write7BitEncodedInt( out, typeReaders.size() );
        for ( const TypeReaderHeader& trh : typeReaders )
            write< TypeReaderHeader >( out, trh );
        
        // TODO: Shared res
        write7BitEncodedInt( out, 0 );
        
        std::size_t index = 0;
        for ( std::size_t i = 1; i < typeReaders.size(); ++i )
        {
            if ( typeReaders[ i ] == data->getTypeReader() )
                index = i;
        }
        std::string typeStr = typeReaders[ index ].name;
        index += 1;
        
        ITypeReader* reader = ITypeReader::getTypeReader( typeStr );
        if ( reader == nullptr )
            throw std::invalid_argument( util::format( "Type not supported: $", typeStr ) );
        
        write7BitEncodedInt( out, index );
        reader->write( ( * this ), out, data.get() );
        
        // TODO: Shared resources
        
        return true;
    }
}
