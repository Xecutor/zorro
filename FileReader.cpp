#include "FileReader.hpp"

namespace zorro {

FileRegistry::~FileRegistry()
{
    for(auto& entry : entries)
    {
        delete entry;
    }
    for(auto& reader : readers)
    {
        delete reader;
    }
}

//FileRegistry freg;
FileReader* FileRegistry::newReader(Entry* argEntry)
{
    auto* rv = new FileReader(this, argEntry);
    readers.push_back(rv);
    return rv;
}

}
