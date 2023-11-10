//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SOURCE_HPP
#define MRDOCS_API_METADATA_SOURCE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <string>
#include <string_view>

namespace clang {
namespace mrdocs {

enum class FileKind
{
    // file in the source directory
    Source,
    // file in a system include directory
    System,
    // file outside the source directory
    Other
};

struct MRDOCS_DECL
    Location
{
    /** The full file path
    */
    std::string Path;

    /** Name of the file
    */
    std::string Filename;

    /** Line number within the file
    */
    unsigned LineNumber = 0;

    /** The kind of file this is
    */
    FileKind Kind = FileKind::Source;

    //--------------------------------------------

    Location(
        std::string_view filepath = "",
        std::string_view filename = "",
        unsigned line = 0,
        FileKind kind = FileKind::Source)
        : Path(filepath)
        , Filename(filename)
        , LineNumber(line)
        , Kind(kind)
    {
    }
};

struct LocationEmptyPredicate
{
    constexpr bool operator()(
        Location const& loc) const noexcept
    {
        return loc.Filename.empty();
    }
};

/** Like std::optional<Location>
*/
using OptionalLocation = Optional<Location, LocationEmptyPredicate>;

/** Stores source information for a declaration.
*/
struct MRDOCS_DECL
    SourceInfo
{
    /** Location where the entity was defined

        KRYSTIAN NOTE: this is used for entities which cannot be
        redeclared -- regardless of whether such a declaration is
        actually a definition (e.g. alias-declarations and
        typedef declarations are never definition).
    */
    OptionalLocation DefLoc;

    /** Locations where the entity was declared,

        This does not include the definition.
    */
    std::vector<Location> Loc;

protected:
    SourceInfo() = default;
};

} // mrdocs
} // clang

#endif
