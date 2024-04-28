cd "$(dirname "$0")"

filename="$(basename "$0")"
binname="../build/aux/${filename%.*}"
cppname="${filename%.*}.cpp"
outname="../build/shared/font.hpp"
inname="../shared/font/font.obj"

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

if ! [ -f "${outname}" ] || [ "${binname}" -nt "${outname}" ] || [ "${filename}" -nt "${outname}" ]; then
    "${binname}" <"${inname}" >"${outname}"
fi

