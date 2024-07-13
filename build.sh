# Copyright 2020-2024 Wafflecat Games, LLC

# This file is part of WITE.

# WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

# WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

# You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

# Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.


BUILDLIBS="WITE"
BUILDTESTS="Tests"
BUILDAPP=""
site_configs="../configs/wite"
WIN_COMPILER= #enable in site_configs by setting to a compiler
if [ -f "$site_configs" ]; then
    source "$site_configs"
else
    echo "warning: site configs not found"
fi;
BOTHOPTS="-DVK_NO_PROTOTYPES -DVULKAN_HPP_NO_EXCEPTIONS"
# -DDO_PROFILE
# -DWITE_DEBUG_IMAGE_BARRIERS
# -DWITE_DEBUG_FENCES
# -DWITE_DEBUG_DB
BASEDIR="$(cd "$(dirname "$0")"; pwd -L)"
OUTDIR="${BASEDIR}/build"
LOGFILE="${OUTDIR}/buildlog.txt"
ERRLOG="${OUTDIR}/builderrs.txt"
ERRLOGBIT="errors.txt"
ERRLOGBITDIR="${OUTDIR}/file_errors"
rm "${LOGFILE}" "${ERRLOG}" ${ERRLOGBITDIR}/*-${ERRLOGBIT} 2>/dev/null
mkdir -p "${OUTDIR}" "${ERRLOGBITDIR}" 2>/dev/null
COMPILER=clang++
WORKNICE="nice -n10"
GLCOMPILER=glslangValidator
TESTOPTIONS="nogpuid=1,2 extent=0,0,3840,2160 presentmode=immediate" #skips llvme pipe on my test system, renders to left monitor
if [ -z "${VK_SDK_PATH}" ]; then
    VK_SDK_PATH="${VULKAN_SDK}"
fi
if [ -z "$VK_INCLUDE" -a -n "$VK_SDK_PATH" ]; then
    VK_INCLUDE="-I${VK_SDK_PATH}/include"
fi
if [ -n "$vk_lib_path" ]; then
    PATH="$vk_lib_path:$PATH"
fi;
LINKOPTS="-fuse-ld=lld -lrt -latomic -lvulkan -lSDL2"
WINLINKOPTS=" /defaultlib:SDL2" #/defaultlib:vulkan
#-L${vk_lib_path}

find "${OUTDIR}" -type f -iname '*.o' -print0 |
    while IFS= read -d '' OFILE; do
	find $BUILDLIBS $BUILDAPP $BUILDTESTS -type f -iname "$(basename "${OFILE%.*}.cpp")" | grep -q . ||
	    rm "${OFILE}" 2>/dev/null;
    done;

find "${BASEDIR}/aux" -type f -name '*.sh' | sort | while read a; do $a >>"${LOGFILE}" 2>>"${ERRLOG}"; done;

if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    #compile shaders first (so they can be imported as c data members)
    find $BUILDAPP $BUILDTESTS -iname '*.glsl' -type f -print0 |
	while IFS= read -d '' SRCFILE && [ $(cat ${ERRLOGBITDIR}/*-${ERRLOGBIT} 2>/dev/null | wc -l) -eq 0 ]; do
	    DSTFILE="${OUTDIR}/${SRCFILE%.*}.spv.h"
	    DSTDIR="$(dirname "$DSTFILE")"
	    test -d "$DSTDIR" || mkdir "$DSTDIR"
	    VARNAME="$(basename "${SRCFILE}" | sed -r 's,/,_,g;s/\.([^.]*)\.glsl$/_\1/')"
	    #TODO are imports or other dependencies possible?
	    THISERRLOG="${ERRLOGBITDIR}/$(echo "${SRCFILE}" | tr '/' '-')-${ERRLOGBIT}"
	    rm "${THISERRLOG}" 2>/dev/null
	    (
		if ! [ -f "${DSTFILE}" ] || [ "${SRCFILE}" -nt "${DSTFILE}" ] || [ "$0" -nt "${DSTFILE}" ]; then
		    #echo building
		    $WORKNICE $GLCOMPILER -V --target-env vulkan1.3 -gVS "${SRCFILE}" -o "${DSTFILE}" --vn "${VARNAME}" || rm "${DSTFILE}" 2>/dev/null
		fi
	    ) 2>&1 | grep -v "^${SRCFILE}$" > "${THISERRLOG}" &
	done
    while pgrep $GLCOMPILER &>/dev/null; do sleep 0.2s; done
fi

if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    #compile to static
    find $BUILDLIBS $BUILDAPP $BUILDTESTS -name '*.cpp' -type f -print0 |
	while IFS= read -d '' SRCFILE && [ $(cat ${ERRLOGBITDIR}/*-${ERRLOGBIT} 2>/dev/null | wc -l) -eq 0 ]; do
	    DSTFILE="${OUTDIR}/${SRCFILE%.*}.o"
	    DSTDIR="$(dirname "$DSTFILE")"
	    test -d "$DSTDIR" || mkdir -p "$DSTDIR"
	    WINDSTFILE="${OUTDIR}/windows/${SRCFILE%.*}.o"
	    WINDSTDIR="$(dirname "$WINDSTFILE")"
	    test -n "$WIN_COMPILER" -a '!' -d "$WINDSTDIR" && mkdir -p "$WINDSTDIR"
	    RELDSTFILE="${OUTDIR}/release/${SRCFILE%.*}.o"
	    RELDSTDIR="$(dirname "$RELDSTFILE")"
	    test -d "$RELDSTDIR" || mkdir -p "$RELDSTDIR"
	    DEPENDENCIES="${OUTDIR}/${SRCFILE%.*}.d"
	    THISERRLOG="${ERRLOGBITDIR}/$(echo "${SRCFILE}" | tr '/' '-')-${ERRLOGBIT}"
	    rm "${THISERRLOG}" 2>/dev/null
	    (
		#if any target needs recompiled, do them all just for a sanity check
		if [ -f "${DSTFILE}" -a "${DSTFILE}" -nt "$0" -a "${DSTFILE}" -nt "${SRCFILE}" -a \
			-f "${RELDSTFILE}" -a "${RELDSTFILE}" -nt "$0" -a "${RELDSTFILE}" -nt "${SRCFILE}" -a \
			'(' -z "$WIN_COMPILER" -o \
			'(' -f "${WINDSTFILE}" -a "${WINDSTFILE}" -nt "$0" -a "${WINDSTFILE}" -nt "${SRCFILE}" ')' ')' ]; then
		    #note: this does not check the win sdk for changes
		    while read depend; do
			if ! [ -f "${depend}" ]; then
			    echo "rebuilding ${SRCFILE} because ${depend} not found (dependencies: ${DEPENDENCIES})";
			    break;
			elif [ "${depend}" -nt "${DSTFILE}" ]; then
			    echo "rebuilding ${SRCFILE} because ${depend} is newer";
			    break;
			fi
		    done <"${DEPENDENCIES}" | grep -Fq rebuild || exit 0;
		else
		    $WORKNICE $COMPILER $VK_INCLUDE -I "${OUTDIR}" -E -o /dev/null -w -ferror-limit=1 -H "${SRCFILE}" 2>&1 | grep '^\.' | grep -o '[^[:space:]]*$' | sort -u >"${DEPENDENCIES}"
		fi
		# -I "${OUTDIR}" is for shaders which get turned into headers
		if ! $WORKNICE $COMPILER $VK_INCLUDE -I "${OUTDIR}" --std=c++20 -fPIC -DDEBUG -g $BOTHOPTS -Werror -Wall "${SRCFILE}" -c -o "${DSTFILE}" >>"${LOGFILE}" 2>>"${THISERRLOG}"; then # -D_POSIX_C_SOURCE=200112L
		    rm "${DSTFILE}" 2>/dev/null
		    echo "Failed Build: ${SRCFILE}" >>"${THISERRLOG}"
		elif ! $WORKNICE $COMPILER $VK_INCLUDE -O3 -I "${OUTDIR}" --std=c++20 -fPIC $BOTHOPTS -Werror -Wall "${SRCFILE}" -c -o "${RELDSTFILE}" >>"${LOGFILE}" 2>>"${THISERRLOG}"; then # -D_POSIX_C_SOURCE=200112L
		    rm "${RELDSTFILE}" 2>/dev/null
		    echo "[release build] Failed Build: ${SRCFILE}" >>"${THISERRLOG}"
		elif [ -n "$WIN_COMPILER" ]; then
		    if ! $WORKNICE $WIN_COMPILER /O2 /std:c++20 -Diswindows $VK_INCLUDE -I "${OUTDIR}" $BOTHOPTS -c "${SRCFILE}" -o "${WINDSTFILE}" >>"${LOGFILE}" 2>>"${THISERRLOG}"; then # -D_POSIX_C_SOURCE=200112L
			rm "${WINDSTFILE}" 2>/dev/null
			echo "[cross compile for windows] Failed Build: ${SRCFILE}" >>"${THISERRLOG}"
		    fi
		fi
	    ) &
	done
fi

let nocompiles=0;
while [ $nocompiles -lt 100 ]; do
    if [ \( -n "$WIN_COMPILER" -a $(pgrep -c "$WIN_COMPILER_RUNNIG_TEST") -gt 0 \) -o $(pgrep -c "$COMPILER") -gt 0 ]; then
	let nocompiles=0;
    else
	let nocompiles++;
    fi;
    sleep 0.01s;
done
cat ${ERRLOGBITDIR}/*-${ERRLOGBIT} >>"${ERRLOG}"

#libs
if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    LIBBUILDDIRS=
    RELLIBBUILDDIRS=
    WINLIBBUILDDIRS=
    for DIRNAME in $BUILDLIBS; do
	LIBBUILDDIRS="${LIBBUILDDIRS} ${OUTDIR}/${DIRNAME}"
	WINLIBBUILDDIRS="${WINLIBBUILDDIRS} ${OUTDIR}/windows/${DIRNAME}"
	RELLIBBUILDDIRS="${RELLIBBUILDDIRS} ${OUTDIR}/release/${DIRNAME}"
    done
    LIBOBJS=$(find $LIBBUILDDIRS -name '*.o')
    WINLIBOBJS=$(find $WINLIBBUILDDIRS -name '*.o')
    RELLIBOBJS=$(find $RELLIBBUILDDIRS -name '*.o')
fi

# #libs
# if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
#     BUILTLIBS=
#     for DIRNAME in $BUILDLIBS; do
# 	BUILTLIBS="${BUILTLIBS} -l:${DIRNAME}.so"
# 	LIBNAME="$OUTDIR/$(basename "${DIRNAME}").so"
# 	WINLIBNAME="$OUTDIR/windows/$(basename "${DIRNAME}").dll"
# 	if ! [ -f "$LIBNAME" -a $(find "$OUTDIR/$DIRNAME" -iname '*.o' -newer "$LIBNAME" | wc -l) -eq 0 ]; then
# 	#echo "Linking $LIBNAME"
# 	    $WORKNICE $COMPILER -shared $BOTHOPTS $LINKOPTS $(find "$OUTDIR/$DIRNAME" -name '*.o') -o $LIBNAME >>"${LOGFILE}" 2>>"${ERRLOG}"
# 	fi;
# 	if [ -n "$WIN_COMPILER" -a ! \( -f "$WINLIBNAME" -a $(find "$OUTDIR/windows/$DIRNAME" -iname '*.o' -newer "$WINLIBNAME" | wc -l) -eq 0 \) ]; then
# 	    $WORKNICE $WIN_LINKER /dll $(find "$OUTDIR/windows/$DIRNAME" -name '*.o') "/out:$WINLIBNAME" >>"${LOGFILE}" 2>>"${ERRLOG}"
# 	fi
#     done
# fi

#tests
if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    for DIRNAME in $BUILDTESTS; do
	find "$OUTDIR/$DIRNAME" -iname '*.o' -print0 |
	    while IFS= read -d '' OFILE; do
		TESTNAME="${OFILE%.*}"
		#test -f "${TESTNAME}" && cp "${TESTNAME}" "${TESTNAME}.bak.$(date '+%y%m%d%H%M')"
		rm "$TESTNAME" 2>/dev/null
		$WORKNICE $COMPILER -DDEBUG -ffunction-sections -fdata-sections "$OFILE" $LIBOBJS -o "$TESTNAME" "-Wl,-rpath,$OUTDIR" $LINKOPTS $BOTHOPTS 2>>"${ERRLOG}" >>"${LOGFILE}"
		grep -qxF "$(basename "${TESTNAME#*/}")" test_skips.txt && continue;
		echo running test "${TESTNAME}" >>"${LOGFILE}"
		# md5sum $TESTNAME >>"${LOGFILE}"
		#VK_LOADER_DEBUG=all
		$WORKNICE time $TESTNAME $TESTOPTIONS 2>>"${ERRLOG}" >>"${LOGFILE}"
		code=$?
		if [ $code -ne 0 ]; then echo "Exit code: " $code >>"${ERRLOG}"; fi;
		#test -f "${TESTNAME}.bak" && ! test -f "${TESTNAME}" && cp "${TESTNAME}.bak" "${TESTNAME}"
	    done
    done
fi

#tests with release build (not ran, available for benchmarking)
if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    for DIRNAME in $BUILDTESTS; do
	find "$OUTDIR/release/$DIRNAME" -iname '*.o' -print0 |
	    while IFS= read -d '' OFILE; do
		TESTNAME="${OFILE%.*}"
		rm "$TESTNAME" 2>/dev/null
		$WORKNICE $COMPILER -O3 -ffunction-sections -fdata-sections "$OFILE" $RELLIBOBJS -o "$TESTNAME" "-Wl,-rpath,$OUTDIR" $LINKOPTS $BOTHOPTS 2>>"${ERRLOG}" >>"${LOGFILE}"
	    done
    done
fi

#win tests (compile but dont run for obvious reasons)
if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    for DIRNAME in $BUILDTESTS; do
	find "$OUTDIR/windows/$DIRNAME" -iname '*.o' -print0 |
	    while IFS= read -d '' OFILE; do
		TESTNAME="${OFILE%.*}"
		rm "$TESTNAME" 2>/dev/null
		#entry mainCRTStartup calls main, so the application (test or real) need not handle main vs wmain
		$WORKNICE $WIN_LINKER /opt:icf=4 /subsystem:windows /entry:mainCRTStartup "$OFILE" $WINLIBOBJS $WINLINKOPTS "/out:${TESTNAME}.exe" 2>>"${ERRLOG}" >>"${LOGFILE}"
	    done
    done
fi

#apps
if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    for DIRNAME in $BUILDAPP; do
	find "$DIRNAME" -iname '*.o' | grep -qF .o || continue;
	APPNAME="$DIRNAME/$(basename "${DIRNAME}").so"
	#TODO VK_LIB ?
	#echo "Linking $APPNAME"
	$WORKNICE $COMPILER $(find "$DIRNAME" -name '*.o') -o $APPNAME $BOTHOPTS >>"${LOGFILE}" 2>>"${ERRLOG}"
    done
fi

if [ -f "${ERRLOG}" ] && [ "$(stat -c %s "${ERRLOG}")" -gt 0 ]; then
    echo "${ERRLOG}"
    cat "${ERRLOG}"
    #echo Failed
else
    echo "Success"
    #tail "${LOGFILE}"
fi

