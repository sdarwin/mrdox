//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/format/FlatWriter.hpp>
#include <llvm/ADT/StringRef.h>
#include <cassert>

namespace clang {
namespace mrdox {

FlatWriter::
FlatWriter(
    llvm::raw_ostream& os,
    llvm::StringRef filePath,
    Corpus const& corpus,
    Reporter& R) noexcept
    : os_(os)
    , filePath_(filePath)
    , corpus_(corpus)
    , R_(R)
{
}

//------------------------------------------------

void
FlatWriter::
visitAllSymbols()
{
    for(auto const& id : corpus_.allSymbols())
        visit(id);
}

void
FlatWriter::
visit(
    SymbolID const& id)
{
    auto const& I = corpus_.get<Info>(id);
    switch(I.IT)
    {
    case InfoType::IT_namespace:
        //visit(*static_cast<NamespaceInfo const*>(&I));
        break;
    case InfoType::IT_record:
        visit(*static_cast<RecordInfo const*>(&I));
        break;
    case InfoType::IT_function:
        visit(*static_cast<FunctionInfo const*>(&I));
        break;
    case InfoType::IT_enum:
    case InfoType::IT_typedef:
        break;
    default:
    case InfoType::IT_default:
        llvm_unreachable("unsupported InfoType");
    }
}

void
FlatWriter::
beginFile()
{
}

void
FlatWriter::
endFile()
{
}

void
FlatWriter::
writeNamespace(
    NamespaceInfo const& I)
{
}

void
FlatWriter::
writeRecord(
    RecordInfo const& I)
{
}

void
FlatWriter::
writeFunction(
    FunctionInfo const& I)
{
}

void
FlatWriter::
writeEnum(
    EnumInfo const& I)
{
}

void
FlatWriter::
writeTypedef(
    TypedefInfo const& I)
{
}

//------------------------------------------------

void
FlatWriter::
visit(
    NamespaceInfo const& I)
{
    writeNamespace(I);
    visit(I.Children);
}

void
FlatWriter::
visit(
    RecordInfo const& I)
{
    writeRecord(I);
}

void
FlatWriter::
visit(
    FunctionInfo const& I)
{
    writeFunction(I);
}

void
FlatWriter::
visit(
    Scope const& scope)
{
    for(auto const& ref : scope.Namespaces)
        visit(ref.id);
    for(auto const& ref : scope.Records)
        visit(ref.id);
    for(auto const& ref : scope.Functions)
        visit(ref.id);
    for(auto const& I : scope.Enums)
        writeEnum(I);
    for(auto const& I : scope.Typedefs)
        writeTypedef(I);
}

} // mrdox
} // clang