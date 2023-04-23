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

#ifndef MRDOX_SOURCE_FORMAT_ASCIIDOC_HPP
#define MRDOX_SOURCE_FORMAT_ASCIIDOC_HPP

#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Generator.hpp>
#include <mrdox/format/FlatWriter.hpp>
#include <mrdox/meta/Javadoc.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

namespace clang {
namespace mrdox {

struct OverloadSet;

class AsciidocGenerator
    : public Generator
{
public:
    class Writer;

    llvm::StringRef
    name() const noexcept override
    {
        return "Asciidoc";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "adoc";
    }

    llvm::Error
    buildSinglePage(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Reporter& R,
        llvm::raw_fd_ostream* fd_os) const override;
};

//------------------------------------------------

class AsciidocGenerator::Writer
    : public FlatWriter
{
    struct Section
    {
        int level = 0;
        std::string markup;
    };

    llvm::raw_fd_ostream* fd_os_;
    Section sect_;

public:
    Writer(
        llvm::raw_ostream& os,
        llvm::raw_fd_ostream* fd_os,
        Corpus const& corpus,
        Reporter& R) noexcept;

    llvm::Error build();

    struct FormalParam;
    struct TypeName;

    void writeFormalParam(FormalParam const& p, llvm::raw_ostream& os);
    void writeTypeName(TypeName const& tn, llvm::raw_ostream& os);

protected:
    void writeRecord(RecordInfo const& I) override;
    void writeFunction(FunctionInfo const& I) override;
    void writeEnum(EnumInfo const& I) override;
    void writeTypedef(TypedefInfo const& I) override;

    void writeBase(BaseRecordInfo const& I);
    void writeFunctionOverloads(
        llvm::StringRef sectionName,
        OverloadsSet const& set);
    void writeNestedTypes(
        llvm::StringRef sectionName,
        std::vector<TypedefInfo> const& list,
        AccessSpecifier access);
    void writeDataMembers(
        llvm::StringRef sectionName,
        llvm::SmallVectorImpl<MemberTypeInfo> const& list,
        AccessSpecifier access);

    void writeBrief(Javadoc::Paragraph const* node, bool withNewline = true);
    void writeLocation(SymbolInfo const& I);
    void writeDescription(List<Javadoc::Block> const& list);

    template<class T>
    void writeNodes(List<T> const& list);
    void writeNode(Javadoc::Node const& node);
    void writeNode(Javadoc::Text const& node);
    void writeNode(Javadoc::StyledText const& node);
    void writeNode(Javadoc::Paragraph const& node);
    void writeNode(Javadoc::Admonition const& node);
    void writeNode(Javadoc::Code const& node);
    void writeNode(Javadoc::Param const& node);
    void writeNode(Javadoc::TParam const& node);
    void writeNode(Javadoc::Returns const& node);

    FormalParam formalParam(FieldTypeInfo const& ft);
    TypeName typeName(TypeInfo const& ti);

    void openSection(llvm::StringRef name);
    void closeSection();

    static llvm::StringRef toString(TagTypeKind k) noexcept;
};

} // mrdox
} // clang

#endif