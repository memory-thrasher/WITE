# Copyright 2020-2025 Wafflecat Games, LLC

# This file is part of WITE.

# WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

# WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

# You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

# Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.

cd "$(dirname "$0")"

filename="$(basename "$0")"
binname="../build/aux/${filename%.*}"
cppname="${filename%.*}.cpp"
outname="../build/shared/font.hpp"
inname="../shared/font/font.obj"

mkdir -p ../build/aux ../build/shared

if ! [ -f "${inname}" ] || ! [ -f "${cppname}" ]; then
    echo missing input files >&2
    if ! [ -f "${inname}" ]; then
	echo "${inname}" >&2
    fi
    if ! [ -f "${cppname}" ]; then
	echo "${cppname}" >&2
    fi
    exit 1;
fi

if ! [ -f "${binname}" ] || [ "${cppname}" -nt "${binname}" ] || [ "${filename}" -nt "${binname}" ]; then
    clang++ --std=c++20 -Werror -Wall -DDEBUG -g "${cppname}" -o "${binname}" || exit 2
fi

if ! [ -f "${outname}" ] || [ "${binname}" -nt "${outname}" ] || [ "${inname}" -nt "${outname}" ] || [ "${filename}" -nt "${outname}" ]; then
    "${binname}" <"${inname}" >"${outname}"
fi

