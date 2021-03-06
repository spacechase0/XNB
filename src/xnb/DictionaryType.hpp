#ifndef XNB_DICTIONARYTYPE_HPP
#define XNB_DICTIONARYTYPE_HPP

#include <map>
#include <memory>
#include <string>

#include "xnb/Data.hpp"
#include "xnb/ITypeReader.hpp"

namespace xnb
{
    class DictionaryData : public Data
    {
        public:
            DictionaryData( const std::string& key, const std::string& value );
            
            const std::string keyType;
            const std::string valueType;
            
            std::map< std::unique_ptr< Data >, std::unique_ptr< Data > > data;
            
            virtual std::string toString() const override;
            
            virtual std::vector< TypeReaderHeader > getTypeReaders() const override;
            virtual std::string getType() const override;
            virtual TypeReaderHeader getTypeReader() const override;
    };
    
    class DictionaryTypeReader : public ITypeReader
    {
        public:
            virtual std::unique_ptr< Data > read( const File& file, std::istream& in, const std::string& fullDecl ) override;
            virtual void write( const File& file, std::ostream& out, const Data* data ) override;
            
            virtual bool resultIsValueType() const override;
            virtual std::string getReaderType() const override;
    };
    
    constexpr const char* DICTIONARY_TYPE = "System.Collections.Generic.Dictionary"; // TODO: Better
    constexpr const char* DICTIONARY_TYPEREADER = "Microsoft.Xna.Framework.Content.DictionaryReader";
}

#endif // XNB_DICTIONARYTYPE_HPP
