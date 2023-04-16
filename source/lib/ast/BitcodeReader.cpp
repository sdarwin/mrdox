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

#include "BitcodeIDs.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/AST/AST.h>
#include <llvm/ADT/IndexedMap.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitstream/BitstreamReader.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

namespace {

//------------------------------------------------

using Record = llvm::SmallVector<uint64_t, 1024>;

// This implements decode for SmallString.
llvm::Error
decodeRecord(
    const Record& R,
    llvm::SmallVectorImpl<char>& Field,
    llvm::StringRef Blob)
{
    Field.assign(Blob.begin(), Blob.end());
    return llvm::Error::success();
}

llvm::Error
decodeRecord(
    Record const& R,
    std::string& Field,
    llvm::StringRef Blob)
{
    Field.assign(Blob.begin(), Blob.end());
    return llvm::Error::success();
}

llvm::Error
decodeRecord(
    const Record& R,
    SymbolID& Field,
    llvm::StringRef Blob)
{
    if (R[0] != BitCodeConstants::USRHashSize)
        return makeError("incorrect USR size");

    // First position in the record is the length of the following array, so we
    // copy the following elements to the field.
    for (int I = 0, E = R[0]; I < E; ++I)
        Field[I] = R[I + 1];
    return llvm::Error::success();
}

llvm::Error
decodeRecord(
    const Record& R,
    bool& Field,
    llvm::StringRef Blob)
{
    Field = R[0] != 0;
    return llvm::Error::success();
}

llvm::Error
decodeRecord(
    const Record& R,
    int& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return makeError("integer too large to parse");
    Field = (int)R[0];
    return llvm::Error::success();
}

llvm::Error
decodeRecord(
    Record const& R,
    AccessSpecifier& Field,
    llvm::StringRef Blob)
{
    switch (R[0])
    {
    case AS_public:
    case AS_private:
    case AS_protected:
    case AS_none:
        Field = (AccessSpecifier)R[0];
        return llvm::Error::success();
    default:
        return makeError("invalid value for AccessSpecifier");
    }
}

llvm::Error
decodeRecord(
    Record const& R,
    TagTypeKind& Field,
    llvm::StringRef Blob)
{
    switch (R[0])
    {
    case TTK_Struct:
    case TTK_Interface:
    case TTK_Union:
    case TTK_Class:
    case TTK_Enum:
        Field = (TagTypeKind)R[0];
        return llvm::Error::success();
    default:
        return makeError("invalid value for TagTypeKind");
    }
}

llvm::Error
decodeRecord(
    Record const& R,
    llvm::Optional<Location>& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return makeError("integer too large to parse");
    Field.emplace((int)R[0], Blob, (bool)R[1]);
    return llvm::Error::success();
}

llvm::Error
decodeRecord(
    Record const& R,
    InfoType& Field,
    llvm::StringRef Blob)
{
    switch (auto IT = static_cast<InfoType>(R[0]))
    {
    case InfoType::IT_namespace:
    case InfoType::IT_record:
    case InfoType::IT_function:
    case InfoType::IT_enum:
    case InfoType::IT_typedef:
    case InfoType::IT_default:
        Field = IT;
        return llvm::Error::success();
    }
    return makeError("invalid value for InfoType");
}

llvm::Error
decodeRecord(
    Record const& R,
    FieldId& Field,
    llvm::StringRef Blob)
{
    switch (auto F = static_cast<FieldId>(R[0]))
    {
    case FieldId::F_namespace:
    case FieldId::F_parent:
    case FieldId::F_vparent:
    case FieldId::F_type:
    case FieldId::F_child_namespace:
    case FieldId::F_child_record:
    case FieldId::F_child_function:
    case FieldId::F_default:
        Field = F;
        return llvm::Error::success();
    }
    return makeError("invalid value for FieldId");
}

llvm::Error
decodeRecord(
    Record const& R,
    llvm::SmallVectorImpl<llvm::SmallString<16>>& Field,
    llvm::StringRef Blob)
{
    Field.push_back(Blob);
    return llvm::Error::success();
}

llvm::Error
decodeRecord(
    const Record& R,
    llvm::SmallVectorImpl<Location>& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return makeError("integer too large to parse");
    Field.emplace_back((int)R[0], Blob, (bool)R[1]);
    return llvm::Error::success();
}

//------------------------------------------------

llvm::Expected<Javadoc*>
getJavadoc(...)
{
    return makeError("invalid type cannot contain Javadoc");
}

llvm::Expected<Javadoc*>
getJavadoc(Info* I)
{
    return &I->javadoc;
}

llvm::Expected<Javadoc*>
getJavadoc(MemberTypeInfo* I)
{
    return &I->javadoc;
}

//------------------------------------------------

// When readSubBlock encounters a TypeInfo sub-block, it calls addTypeInfo on
// the parent block to set it. The template specializations define what to do
// for each supported parent block.
template <typename T, typename TTypeInfo>
llvm::Error
addTypeInfo(
    T I, TTypeInfo&& TI)
{
    return makeError("invalid type cannot contain TypeInfo");
}

template<> llvm::Error addTypeInfo(RecordInfo* I, MemberTypeInfo&& T) {
    I->Members.emplace_back(std::move(T));
    return llvm::Error::success();
}

template<> llvm::Error addTypeInfo(BaseRecordInfo* I, MemberTypeInfo&& T) {
    I->Members.emplace_back(std::move(T));
    return llvm::Error::success();
}

template<> llvm::Error addTypeInfo(FunctionInfo* I, TypeInfo&& T) {
    I->ReturnType = std::move(T);
    return llvm::Error::success();
}

template<> llvm::Error addTypeInfo(FunctionInfo* I, FieldTypeInfo&& T) {
    I->Params.emplace_back(std::move(T));
    return llvm::Error::success();
}

template<> llvm::Error addTypeInfo(EnumInfo* I, TypeInfo&& T) {
    I->BaseType = std::move(T);
    return llvm::Error::success();
}

template<> llvm::Error addTypeInfo(TypedefInfo* I, TypeInfo&& T) {
    I->Underlying = std::move(T);
    return llvm::Error::success();
}

template <typename T>
llvm::Error
addReference(
    T I,
    Reference&& R,
    FieldId F)
{
    return makeError("invalid type cannot contain Reference");
}

template<>
llvm::Error
addReference(
    TypeInfo* I,
    Reference&& R,
    FieldId F)
{
    switch (F)
    {
    case FieldId::F_type:
        I->Type = std::move(R);
        return llvm::Error::success();
    default:
        return makeError("invalid type cannot contain Reference");
    }
}

template<>
llvm::Error
addReference(
    FieldTypeInfo* I,
    Reference&& R,
    FieldId F)
{
    switch (F) {
    case FieldId::F_type:
        I->Type = std::move(R);
        return llvm::Error::success();
    default:
        return makeError("invalid type cannot contain Reference");
    }
}

template<>
llvm::Error
addReference(
    MemberTypeInfo* I,
    Reference&& R,
    FieldId F)
{
    switch (F)
    {
    case FieldId::F_type:
        I->Type = std::move(R);
        return llvm::Error::success();
    default:
        return makeError("invalid type cannot contain Reference");
    }
}

template<>
llvm::Error
addReference(
    EnumInfo* I,
    Reference&& R,
    FieldId F)
{
    switch (F)
    {
    case FieldId::F_namespace:
        I->Namespace.emplace_back(std::move(R));
        return llvm::Error::success();
    default:
        return makeError("invalid type cannot contain Reference");
    }
}

template<>
llvm::Error
addReference(
    TypedefInfo* I,
    Reference&& R,
    FieldId F)
{
    switch (F)
    {
    case FieldId::F_namespace:
        I->Namespace.emplace_back(std::move(R));
        return llvm::Error::success();
    default:
        return makeError("invalid type cannot contain Reference");
    }
}

template<>
llvm::Error
addReference(
    NamespaceInfo* I,
    Reference&& R,
    FieldId F)
{
    switch (F)
    {
    case FieldId::F_namespace:
        I->Namespace.emplace_back(std::move(R));
        return llvm::Error::success();
    case FieldId::F_child_namespace:
        I->Children.Namespaces.emplace_back(std::move(R));
        return llvm::Error::success();
    case FieldId::F_child_record:
        I->Children.Records.emplace_back(std::move(R));
        return llvm::Error::success();
    case FieldId::F_child_function:
        I->Children.Functions.emplace_back(std::move(R));
        return llvm::Error::success();
    default:
        return makeError("invalid type cannot contain Reference");
    }
}

template<>
llvm::Error
addReference(
    FunctionInfo* I,
    Reference&& R,
    FieldId F)
{
    switch (F)
    {
    case FieldId::F_namespace:
        I->Namespace.emplace_back(std::move(R));
        return llvm::Error::success();
    case FieldId::F_parent:
        I->Parent = std::move(R);
        return llvm::Error::success();
    default:
        return makeError("invalid type cannot contain Reference");
    }
}

template<>
llvm::Error
addReference(
    RecordInfo* I,
    Reference&& R,
    FieldId F)
{
    switch (F)
    {
    case FieldId::F_namespace:
        I->Namespace.emplace_back(std::move(R));
        return llvm::Error::success();
    case FieldId::F_parent:
        I->Parents.emplace_back(std::move(R));
        return llvm::Error::success();
    case FieldId::F_vparent:
        I->VirtualParents.emplace_back(std::move(R));
        return llvm::Error::success();
    case FieldId::F_child_record:
        I->Children.Records.emplace_back(std::move(R));
        return llvm::Error::success();
    case FieldId::F_child_function:
        I->Children.Functions.emplace_back(std::move(R));
        return llvm::Error::success();
    default:
        return makeError("invalid type cannot contain Reference");
    }
}

template <typename T, typename ChildInfoType>
void
addChild(T I, ChildInfoType&& R)
{
    llvm::errs() << "invalid child type for info";
    // VFALCO Surely we can do better than this?
    exit(1);
}

// Namespace children:

template<>
void
addChild(
    NamespaceInfo* I,
    EnumInfo&& R)
{
    I->Children.Enums.emplace_back(std::move(R));
}

template<>
void
addChild(
    NamespaceInfo* I,
    TypedefInfo&& R)
{
    I->Children.Typedefs.emplace_back(std::move(R));
}

// Record children:

template<>
void
addChild(
    RecordInfo* I,
    EnumInfo&& R)
{
    I->Children.Enums.emplace_back(std::move(R));
}

template<>
void
addChild(
    RecordInfo* I,
    TypedefInfo&& R)
{
    I->Children.Typedefs.emplace_back(std::move(R));
}

// Other types of children:
template<>
void
addChild(
    EnumInfo* I,
    EnumValueInfo&& R)
{
    I->Members.emplace_back(std::move(R));
}

template<>
void
addChild(
    RecordInfo* I,
    BaseRecordInfo&& R)
{
    I->Bases.emplace_back(std::move(R));
}

// TemplateParam children. These go into either a TemplateInfo (for template
// parameters) or TemplateSpecializationInfo (for the specialization's
// parameters).
template<typename T>
void
addTemplateParam(
    T I,
    TemplateParamInfo&& P)
{
    llvm::errs() << "invalid container for template parameter";
    exit(1);
}
template<>
void
addTemplateParam(
    TemplateInfo* I,
    TemplateParamInfo&& P)
{
    I->Params.emplace_back(std::move(P));
}

template<>
void
addTemplateParam(
    TemplateSpecializationInfo* I,
    TemplateParamInfo&& P)
{
    I->Params.emplace_back(std::move(P));
}

// Template info. These apply to either records or functions.
template<typename T>
void
addTemplate(
    T I,
    TemplateInfo&& P)
{
    llvm::errs() << "invalid container for template info";
    exit(1);
}

template<>
void
addTemplate(
    RecordInfo* I,
    TemplateInfo&& P)
{
    I->Template.emplace(std::move(P));
}

template<>
void
addTemplate(
    FunctionInfo* I,
    TemplateInfo&& P)
{
    I->Template.emplace(std::move(P));
}

// Template specializations go only into template records.
template <typename T>
void
addTemplateSpecialization(
    T I,
    TemplateSpecializationInfo&& TSI)
{
    llvm::errs() << "invalid container for template specialization info";
    exit(1);
}

template<>
void
addTemplateSpecialization(
    TemplateInfo* I,
    TemplateSpecializationInfo&& TSI)
{
    I->Specialization.emplace(std::move(TSI));
}

//------------------------------------------------

// Class to read bitstream into an InfoSet collection
class BitcodeReader
{
public:
    BitcodeReader(
        llvm::BitstreamCursor& Stream,
        Reporter& R)
        : R_(R)
        , Stream(Stream)
    {
    }

    // Main entry point, calls readBlock to read each block in the given stream.
    llvm::Expected<
        std::vector<std::unique_ptr<Info>>>
    getInfos();

private:
    enum class Cursor
    {
        BadBlock = 1,
        Record,
        BlockEnd,
        BlockBegin
    };

    llvm::Error validateStream();
    llvm::Error readBlockInfoBlock();

    /** Read the next Info

        Calls createInfo after casting.
    */
    llvm::Expected<std::unique_ptr<Info>>
    readBlockToInfo(unsigned ID);

    /** Return T from reading the stream.
    */
    template <typename T>
    llvm::Expected<std::unique_ptr<Info>>
    createInfo(unsigned ID);

    /** Read a single block.

        Calls readRecord on each record found.
    */
    template<typename T>
    llvm::Error
    readBlock(unsigned ID, T I);

    // Step through a block of records to find the next data field.
    template<typename T>
    llvm::Error
    readSubBlock(unsigned ID, T I);

    /** Read a record into a data field.

        This calls parseRecord after casting.
    */
    template<typename T>
    llvm::Error
    readRecord(unsigned ID, T I);

    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, const unsigned VersionNo);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, NamespaceInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, RecordInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, BaseRecordInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, FunctionInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, EnumInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, EnumValueInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, TypedefInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, TypeInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, FieldTypeInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, MemberTypeInfo* I);
    llvm::Error parseRecord(const Record& R, unsigned ID,
        llvm::StringRef Blob, Reference* I, FieldId& F);
    llvm::Error parseRecord(const Record& R, unsigned ID,
        llvm::StringRef Blob, TemplateInfo* I);
    llvm::Error parseRecord(const Record& R, unsigned ID,
        llvm::StringRef Blob, TemplateSpecializationInfo* I);
    llvm::Error parseRecord(const Record& R, unsigned ID,
        llvm::StringRef Blob, TemplateParamInfo* I);

    struct JavadocCtorParams
    {
        llvm::SmallString<32> brief;
        std::string desc;

        List<Javadoc::Block> blocks;
        List<Javadoc::Param> params;
        List<Javadoc::TParam> tparams;
        Javadoc::Returns returns;
    };

    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, JavadocCtorParams* init);

    template<class T>
    llvm::Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob,
        List<T>* list);

    // Used to construct a Javadoc

    // Helper function to step through blocks to find and dispatch the next record
    // or block to be read.
    Cursor skipUntilRecordOrBlock(unsigned &BlockOrRecordID);

private:
    Reporter& R_;
    llvm::BitstreamCursor &Stream;
    llvm::Optional<llvm::BitstreamBlockInfo> BlockInfo;
    FieldId CurrentReferenceField;
};

//------------------------------------------------

// Entry point
llvm::Expected<
    std::vector<std::unique_ptr<Info>>>
BitcodeReader::
getInfos()
{
    std::vector<std::unique_ptr<Info>> Infos;
    if (auto Err = validateStream())
        return std::move(Err);

    // Read the top level blocks.
    while (!Stream.AtEndOfStream())
    {
        Expected<unsigned> MaybeCode = Stream.ReadCode();
        if (!MaybeCode)
            return MaybeCode.takeError();
        if (MaybeCode.get() != llvm::bitc::ENTER_SUBBLOCK)
            return makeError("no blocks in input");
        Expected<unsigned> MaybeID = Stream.ReadSubBlockID();
        if (!MaybeID)
            return MaybeID.takeError();
        unsigned ID = MaybeID.get();
        switch (ID)
        {
        // NamedType and Comment blocks should
        // not appear at the top level
        case BI_TYPE_BLOCK_ID:
        case BI_FIELD_TYPE_BLOCK_ID:
        case BI_MEMBER_TYPE_BLOCK_ID:
        case BI_JAVADOC_BLOCK_ID:
        case BI_REFERENCE_BLOCK_ID:
            return makeError("invalid top level block");
        case BI_NAMESPACE_BLOCK_ID:
        case BI_RECORD_BLOCK_ID:
        case BI_FUNCTION_BLOCK_ID:
        case BI_ENUM_BLOCK_ID:
        case BI_TYPEDEF_BLOCK_ID:
        {
            auto InfoOrErr = readBlockToInfo(ID);
            if (!InfoOrErr)
                return InfoOrErr.takeError();
            Infos.emplace_back(std::move(InfoOrErr.get()));
            continue;
        }
        case BI_VERSION_BLOCK_ID:
            if (auto Err = readBlock(ID, VersionNumber))
                return std::move(Err);
            continue;
        case llvm::bitc::BLOCKINFO_BLOCK_ID:
            if (auto Err = readBlockInfoBlock())
                return std::move(Err);
            continue;
        default:
            if (llvm::Error Err = Stream.SkipBlock()) {
                // FIXME this drops the error on the floor.
                consumeError(std::move(Err));
            }
            continue;
        }
    }
    return std::move(Infos);
}

//------------------------------------------------

llvm::Error
BitcodeReader::
validateStream()
{
    if (Stream.AtEndOfStream())
        return makeError("premature end of stream");

    // Sniff for the signature.
    for (int i = 0; i != 4; ++i)
    {
        Expected<llvm::SimpleBitstreamCursor::word_t> MaybeRead = Stream.Read(8);
        if (!MaybeRead)
            return MaybeRead.takeError();
        else if (MaybeRead.get() != BitCodeConstants::Signature[i])
            return makeError("invalid bitcode signature");
    }
    return llvm::Error::success();
}

llvm::Error
BitcodeReader::
readBlockInfoBlock()
{
    Expected<llvm::Optional<llvm::BitstreamBlockInfo>> MaybeBlockInfo =
        Stream.ReadBlockInfoBlock();
    if (!MaybeBlockInfo)
        return MaybeBlockInfo.takeError();
    BlockInfo = MaybeBlockInfo.get();
    if (!BlockInfo)
        return makeError("unable to parse BlockInfoBlock");
    Stream.setBlockInfo(&*BlockInfo);
    return llvm::Error::success();
}

llvm::Expected<std::unique_ptr<Info>>
BitcodeReader::
readBlockToInfo(
    unsigned ID)
{
    switch (ID)
    {
    case BI_NAMESPACE_BLOCK_ID:
        return createInfo<NamespaceInfo>(ID);
    case BI_RECORD_BLOCK_ID:
        return createInfo<RecordInfo>(ID);
    case BI_FUNCTION_BLOCK_ID:
        return createInfo<FunctionInfo>(ID);
    case BI_ENUM_BLOCK_ID:
        return createInfo<EnumInfo>(ID);
    case BI_TYPEDEF_BLOCK_ID:
        return createInfo<TypedefInfo>(ID);
    default:
        return makeError("cannot create info");
    }
}

template <typename T>
llvm::Expected<std::unique_ptr<Info>>
BitcodeReader::
createInfo(unsigned ID)
{
    std::unique_ptr<Info> I = std::make_unique<T>();
    if (auto Err = readBlock(ID, static_cast<T*>(I.get())))
        return std::move(Err);
    return std::unique_ptr<Info>{std::move(I)};
}

//------------------------------------------------

// Read a block of records into a single info.
template <typename T>
llvm::Error
BitcodeReader::
readBlock(unsigned ID, T I)
{
    if (llvm::Error Err = Stream.EnterSubBlock(ID))
        return Err;

    for(;;)
    {
        unsigned BlockOrCode = 0;
        Cursor Res = skipUntilRecordOrBlock(BlockOrCode);

        switch (Res)
        {
        case Cursor::BadBlock:
            return makeError("bad block found");
        case Cursor::BlockEnd:
            return llvm::Error::success();
        case Cursor::BlockBegin:
            if (llvm::Error Err = readSubBlock(BlockOrCode, I))
            {
                if (llvm::Error Skipped = Stream.SkipBlock())
                    return joinErrors(std::move(Err), std::move(Skipped));
                return Err;
            }
            continue;
        case Cursor::Record:
            break;
        }
        if (auto Err = readRecord(BlockOrCode, I))
            return Err;
    }
}

template<typename T>
llvm::Error
BitcodeReader::
readSubBlock(
    unsigned ID, T I)
{
    // Blocks can only have certain types of sub blocks.
    switch (ID)
    {
    case BI_JAVADOC_BLOCK_ID:
    {
        auto rv = getJavadoc(I);
        if(! rv)
            return rv.takeError();
        JavadocCtorParams init;
        if (auto Err = readBlock(ID, &init))
            return Err;
        rv.get()->brief = init.brief;
        rv.get()->desc = init.desc;
        *rv.get() = Javadoc(
            std::move(init.blocks),
            std::move(init.params),
            std::move(init.tparams),
            std::move(init.returns));
        return llvm::Error::success();
    }

    case BI_TYPE_BLOCK_ID:
    {
        TypeInfo TI;
        if (auto Err = readBlock(ID, &TI))
            return Err;
        if (auto Err = addTypeInfo(I, std::move(TI)))
            return Err;
        return llvm::Error::success();
    }
    case BI_FIELD_TYPE_BLOCK_ID:
    {
        FieldTypeInfo TI;
        if (auto Err = readBlock(ID, &TI))
            return Err;
        if (auto Err = addTypeInfo(I, std::move(TI)))
            return Err;
        return llvm::Error::success();
    }
    case BI_MEMBER_TYPE_BLOCK_ID:
    {
        MemberTypeInfo TI;
        if (auto Err = readBlock(ID, &TI))
            return Err;
        if (auto Err = addTypeInfo(I, std::move(TI)))
            return Err;
        return llvm::Error::success();
    }
    case BI_REFERENCE_BLOCK_ID:
    {
        Reference R;
        if (auto Err = readBlock(ID, &R))
            return Err;
        if (auto Err = addReference(I, std::move(R), CurrentReferenceField))
            return Err;
        return llvm::Error::success();
    }
    case BI_BASE_RECORD_BLOCK_ID:
    {
        BaseRecordInfo BR;
        if (auto Err = readBlock(ID, &BR))
            return Err;
        addChild(I, std::move(BR));
        return llvm::Error::success();
    }
    case BI_ENUM_BLOCK_ID:
    {
        EnumInfo E;
        if (auto Err = readBlock(ID, &E))
            return Err;
        addChild(I, std::move(E));
        return llvm::Error::success();
    }
    case BI_ENUM_VALUE_BLOCK_ID:
    {
        EnumValueInfo EV;
        if (auto Err = readBlock(ID, &EV))
            return Err;
        addChild(I, std::move(EV));
        return llvm::Error::success();
    }
    case BI_TEMPLATE_BLOCK_ID:
    {
        TemplateInfo TI;
        if (auto Err = readBlock(ID, &TI))
            return Err;
        addTemplate(I, std::move(TI));
        return llvm::Error::success();
    }
    case BI_TEMPLATE_SPECIALIZATION_BLOCK_ID:
    {
        TemplateSpecializationInfo TSI;
        if (auto Err = readBlock(ID, &TSI))
            return Err;
        addTemplateSpecialization(I, std::move(TSI));
        return llvm::Error::success();
    }
    case BI_TEMPLATE_PARAM_BLOCK_ID:
    {
        TemplateParamInfo TPI;
        if (auto Err = readBlock(ID, &TPI))
            return Err;
        addTemplateParam(I, std::move(TPI));
        return llvm::Error::success();
    }
    case BI_TYPEDEF_BLOCK_ID:
    {
        TypedefInfo TI;
        if (auto Err = readBlock(ID, &TI))
            return Err;
        addChild(I, std::move(TI));
        return llvm::Error::success();
    }
    default:
        return makeError("invalid subblock type");
    }
}

// Read records from bitcode into a given info.
template <typename T>
llvm::Error
BitcodeReader::
readRecord(
    unsigned ID, T I)
{
    Record R;
    llvm::StringRef Blob;
    llvm::Expected<unsigned> MaybeRecID = Stream.readRecord(ID, R, &Blob);
    if (!MaybeRecID)
        return MaybeRecID.takeError();
    return parseRecord(R, MaybeRecID.get(), Blob, I);
}

template<>
llvm::Error
BitcodeReader::
readRecord(
    unsigned ID,
    Reference* I)
{
    Record R;
    llvm::StringRef Blob;
    llvm::Expected<unsigned> MaybeRecID = Stream.readRecord(ID, R, &Blob);
    if (!MaybeRecID)
        return MaybeRecID.takeError();
    return parseRecord(R, MaybeRecID.get(), Blob, I, CurrentReferenceField);
}

//------------------------------------------------

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    const unsigned VersionNo)
{
    if (ID == VERSION && R[0] == VersionNo)
        return llvm::Error::success();
    return makeError("mismatched bitcode version number");
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    NamespaceInfo* I)
{
    switch (ID)
    {
    case NAMESPACE_USR:
        return decodeRecord(R, I->USR, Blob);
    case NAMESPACE_NAME:
        return decodeRecord(R, I->Name, Blob);
    case NAMESPACE_PATH:
        return decodeRecord(R, I->Path, Blob);
    default:
        return makeError("invalid field for NamespaceInfo");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    RecordInfo* I)
{
    switch (ID)
    {
    case RECORD_USR:
        return decodeRecord(R, I->USR, Blob);
    case RECORD_NAME:
        return decodeRecord(R, I->Name, Blob);
    case RECORD_PATH:
        return decodeRecord(R, I->Path, Blob);
    case RECORD_DEFLOCATION:
        return decodeRecord(R, I->DefLoc, Blob);
    case RECORD_LOCATION:
        return decodeRecord(R, I->Loc, Blob);
    case RECORD_TAG_TYPE:
        return decodeRecord(R, I->TagType, Blob);
    case RECORD_IS_TYPE_DEF:
        return decodeRecord(R, I->IsTypeDef, Blob);
    default:
        return makeError("invalid field for RecordInfo");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    BaseRecordInfo* I)
{
    switch (ID)
    {
    case BASE_RECORD_USR:
        return decodeRecord(R, I->USR, Blob);
    case BASE_RECORD_NAME:
        return decodeRecord(R, I->Name, Blob);
    case BASE_RECORD_PATH:
        return decodeRecord(R, I->Path, Blob);
    case BASE_RECORD_TAG_TYPE:
        return decodeRecord(R, I->TagType, Blob);
    case BASE_RECORD_IS_VIRTUAL:
        return decodeRecord(R, I->IsVirtual, Blob);
    case BASE_RECORD_ACCESS:
        return decodeRecord(R, I->Access, Blob);
    case BASE_RECORD_IS_PARENT:
        return decodeRecord(R, I->IsParent, Blob);
    default:
        return makeError("invalid field for BaseRecordInfo");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    FunctionInfo* I)
{
    switch (ID)
    {
    case FUNCTION_USR:
        return decodeRecord(R, I->USR, Blob);
    case FUNCTION_NAME:
        return decodeRecord(R, I->Name, Blob);
    case FUNCTION_DEFLOCATION:
        return decodeRecord(R, I->DefLoc, Blob);
    case FUNCTION_LOCATION:
        return decodeRecord(R, I->Loc, Blob);
    case FUNCTION_ACCESS:
        return decodeRecord(R, I->Access, Blob);
    case FUNCTION_IS_METHOD:
        return decodeRecord(R, I->IsMethod, Blob);
    default:
        return makeError("invalid field for FunctionInfo");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    EnumInfo* I)
{
    switch (ID)
    {
    case ENUM_USR:
        return decodeRecord(R, I->USR, Blob);
    case ENUM_NAME:
        return decodeRecord(R, I->Name, Blob);
    case ENUM_DEFLOCATION:
        return decodeRecord(R, I->DefLoc, Blob);
    case ENUM_LOCATION:
        return decodeRecord(R, I->Loc, Blob);
    case ENUM_SCOPED:
        return decodeRecord(R, I->Scoped, Blob);
    default:
        return makeError("invalid field for EnumInfo");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    EnumValueInfo* I)
{
    switch (ID)
    {
    case ENUM_VALUE_NAME:
        return decodeRecord(R, I->Name, Blob);
    case ENUM_VALUE_VALUE:
        return decodeRecord(R, I->Value, Blob);
    case ENUM_VALUE_EXPR:
        return decodeRecord(R, I->ValueExpr, Blob);
    default:
        return makeError("invalid field for EnumValueInfo");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    TypedefInfo* I)
{
    switch (ID)
    {
    case TYPEDEF_USR:
        return decodeRecord(R, I->USR, Blob);
    case TYPEDEF_NAME:
        return decodeRecord(R, I->Name, Blob);
    case TYPEDEF_DEFLOCATION:
        return decodeRecord(R, I->DefLoc, Blob);
    case TYPEDEF_IS_USING:
        return decodeRecord(R, I->IsUsing, Blob);
    default:
        return makeError("invalid field for TypedefInfo");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    TypeInfo* I)
{
    return llvm::Error::success();
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    FieldTypeInfo* I)
{
    switch (ID)
    {
    case FIELD_TYPE_NAME:
        return decodeRecord(R, I->Name, Blob);
    case FIELD_DEFAULT_VALUE:
        return decodeRecord(R, I->DefaultValue, Blob);
    default:
        return makeError("invalid field for TypeInfo");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    MemberTypeInfo* I)
{
    switch (ID)
    {
    case MEMBER_TYPE_NAME:
        return decodeRecord(R, I->Name, Blob);
    case MEMBER_TYPE_ACCESS:
        return decodeRecord(R, I->Access, Blob);
    default:
        return makeError("invalid field for MemberTypeInfo");
    }
}

template<class T>
llvm::Error
BitcodeReader::
parseRecord(
    Record const& R,
    unsigned ID,
    llvm::StringRef Blob,
    List<T>* list)
{
    return llvm::Error::success();
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    Reference* I,
    FieldId& F)
{
    switch (ID)
    {
    case REFERENCE_USR:
        return decodeRecord(R, I->USR, Blob);
    case REFERENCE_NAME:
        return decodeRecord(R, I->Name, Blob);
    case REFERENCE_TYPE:
        return decodeRecord(R, I->RefType, Blob);
    case REFERENCE_PATH:
        return decodeRecord(R, I->Path, Blob);
    case REFERENCE_FIELD:
        return decodeRecord(R, F, Blob);
    default:
        return makeError("invalid field for Reference");
    }
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    TemplateInfo* I)
{
    // Currently there are no child records of TemplateInfo (only child blocks).
    return makeError("invalid field for TemplateParamInfo");
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    TemplateSpecializationInfo* I)
{
    if (ID == TEMPLATE_SPECIALIZATION_OF)
        return decodeRecord(R, I->SpecializationOf, Blob);
    return makeError("invalid field for TemplateParamInfo");
}

llvm::Error
BitcodeReader::
parseRecord(
    const Record& R,
    unsigned ID,
    llvm::StringRef Blob,
    TemplateParamInfo* I)
{
    if (ID == TEMPLATE_PARAM_CONTENTS)
        return decodeRecord(R, I->Contents, Blob);
    return makeError("invalid field for TemplateParamInfo");
}

llvm::Error
BitcodeReader::
parseRecord(
    Record const& R,
    unsigned ID,
    llvm::StringRef Blob,
    JavadocCtorParams* init)
{
    switch (ID)
    {
    case JAVADOC_NODE_KIND:
    {
        int kind;
        return decodeRecord(R, kind, Blob);
    }
    default:
        return makeError("invalid field for Javadoc");
    }
}

//------------------------------------------------

auto
BitcodeReader::
skipUntilRecordOrBlock(
    unsigned& BlockOrRecordID) ->
        Cursor
{
    BlockOrRecordID = 0;

    while (!Stream.AtEndOfStream())
    {
        Expected<unsigned> MaybeCode = Stream.ReadCode();
        if (!MaybeCode)
        {
            // FIXME this drops the error on the floor.
            consumeError(MaybeCode.takeError());
            return Cursor::BadBlock;
        }

        unsigned Code = MaybeCode.get();
        if (Code >= static_cast<unsigned>(llvm::bitc::FIRST_APPLICATION_ABBREV))
        {
            BlockOrRecordID = Code;
            return Cursor::Record;
        }
        switch (static_cast<llvm::bitc::FixedAbbrevIDs>(Code))
        {
        case llvm::bitc::ENTER_SUBBLOCK:
            if (Expected<unsigned> MaybeID = Stream.ReadSubBlockID())
                BlockOrRecordID = MaybeID.get();
            else {
                // FIXME this drops the error on the floor.
                consumeError(MaybeID.takeError());
            }
            return Cursor::BlockBegin;
        case llvm::bitc::END_BLOCK:
            if (Stream.ReadBlockEnd())
                return Cursor::BadBlock;
            return Cursor::BlockEnd;
        case llvm::bitc::DEFINE_ABBREV:
            if (llvm::Error Err = Stream.ReadAbbrevRecord()) {
                // FIXME this drops the error on the floor.
                consumeError(std::move(Err));
            }
            continue;
        case llvm::bitc::UNABBREV_RECORD:
            return Cursor::BadBlock;
        case llvm::bitc::FIRST_APPLICATION_ABBREV:
            llvm_unreachable("Unexpected abbrev id.");
        }
    }
    llvm_unreachable("Premature stream end.");
}

//------------------------------------------------

} // (anon)

// Calls readBlock to read each block in the given stream.
llvm::Expected<
    std::vector<std::unique_ptr<Info>>>
readBitcode(
    llvm::BitstreamCursor &Stream,
    Reporter& R)
{
    BitcodeReader reader(Stream, R);
    return reader.getInfos();
}

} // mrdox
} // clang
