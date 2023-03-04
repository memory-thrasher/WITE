BUILDLIBS="WITE"
BUILDTESTS="Tests"
BUILDAPP=""
LINKOPTS="-lrt -latomic -lvulkan"
BOTHOPTS="-pthread -DDEBUG -g"
BASEDIR="$(cd "$(dirname "$0")"; pwd -L)"
OUTDIR="${BASEDIR}/build" #TODO not just WITE but all subdirs
LOGFILE="${OUTDIR}/buildlog.txt"
ERRLOG="${OUTDIR}/builderrs.txt"
ERRLOGBIT="errors.txt"
ERRLOGBITDIR="${OUTDIR}/file_errors"
rm "${LOGFILE}" "${ERRLOG}" ${ERRLOGBITDIR}/*-${ERRLOGBIT} 2>/dev/null
mkdir -p "${OUTDIR}" "${ERRLOGBITDIR}" 2>/dev/null
if [ -z "${VK_SDK_PATH}" ]; then
    VK_SDK_PATH="${VULKAN_SDK}"
fi
COMPILER=clang++
WORKNICE="nice -n10"

#config
if [ -z "$VK_INCLUDE" -a ! -z "$VK_SDK_PATH" ]; then
    VK_INCLUDE="-I${VK_SDK_PATH}/Include -I${VK_SDK_PATH}/Third-Party/Include"
fi

#compile to static
find $BUILDLIBS $BUILDAPP $BUILDTESTS -name '*.cpp' -type f -print0 |
    while IFS= read -d '' SRCFILE && [ $(cat ${ERRLOGBITDIR}/*-${ERRLOGBIT} 2>/dev/null | wc -l) -eq 0 ]; do
	if [ $(cat 2>/dev/null ${ERRLOGBITDIR}/*-${ERRLOGBIT} 2>/dev/null | wc -l) -gt 0 ]; then break; fi;
	DSTFILE="${OUTDIR}/${SRCFILE%.*}.o"
	DEPENDENCIES="${OUTDIR}/${SRCFILE%.*}.d"
	THISERRLOG="${ERRLOGBITDIR}/$(echo "${SRCFILE}" | tr '/' '-')-${ERRLOGBIT}"
	rm "${THISERRLOG}" 2>/dev/null
	mkdir -p "$(dirname "$DSTFILE")";
	(
	    if [ -f "${DSTFILE}" ] && [ "${DSTFILE}" -nt "$0" ] && [ "${DSTFILE}" -nt "${SRCFILE}" ]; then
		if ! test -f "${DEPENDENCIES}" || test "${SRCFILE}" -nt "${DEPENDENCIES}"; then
		    $WORKNICE $COMPILER $VK_INCLUDE -E -o /dev/null -w -ferror-limit=1 -H "${SRCFILE}" 2>&1 | grep '^\.' | grep -o '[^[:space:]]*$' | sort -u &>"${DEPENDENCIES}"
		fi
		while read depend; do
		    if ! [ -f "${depend}" ]; then
			echo "rebuilding ${SRCFILE} because ${depend} not found (dependencies: ${DEPENDENCIES})";
			break;
		    elif [ "${depend}" -nt "${DSTFILE}" ]; then
			echo "rebuilding ${SRCFILE} because ${depend} is newer";
			break;
		    fi
		done <"${DEPENDENCIES}" | grep -F rebuild || exit 0;
	    else
		echo "building ${DSTFILE}"
	    fi
	    rm "${DEPENDENCIES}" &>/dev/null
	    if ! $WORKNICE $COMPILER $VK_INCLUDE --std=c++20 -D_POSIX_C_SOURCE=200112L -fPIC $BOTHOPTS -Werror -Wall "${SRCFILE}" -c -o "${DSTFILE}" >>"${LOGFILE}" 2>>"${THISERRLOG}"; then
		touch "${SRCFILE}";
		echo "Failed Build: ${SRCFILE}"
	    else
		echo "Built: ${SRCFILE}"
	    fi
	) &
    done

#TODO other file types (shaders etc.)

while pgrep $COMPILER &>/dev/null && [ $(cat 2>/dev/null ${ERRLOGBITDIR}/*-${ERRLOGBIT} | wc -l) -eq 0 ]; do sleep 0.2s; done
sleep 1s;
cat ${ERRLOGBITDIR}/*-${ERRLOGBIT} >"${ERRLOG}"

#libs
if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    BUILTLIBS=
    for DIRNAME in $BUILDLIBS; do
	BUILTLIBS="${BUILTLIBS} -l:${DIRNAME}.so"
	LIBNAME="$OUTDIR/$(basename "${DIRNAME}").so"
	if [ -f "$LIBNAME" ] && [ $(find "$OUTDIR/$DIRNAME" -iname '*.o' -newer "$LIBNAME" | wc -l) -eq 0 ]; then continue; fi;
	#TODO VK_LIB ?
	test -f "$OUTDIR/${DIRNAME}.so" && cp "$OUTDIR/${DIRNAME}.so" "$OUTDIR/${DIRNAME}.so.bak.$(date '+%y%m%d%H%M')"
	echo "Linking $LIBNAME"
	$WORKNICE $COMPILER -shared $BOTHOPTS $LINKOPTS $(find "$OUTDIR/$DIRNAME" -name '*.o') -o $LIBNAME >>"${LOGFILE}" 2>>"${ERRLOG}"
    done
fi

#tests
if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    for DIRNAME in $BUILDTESTS; do
	find "$OUTDIR/$DIRNAME" -iname '*.o' -print0 |
	    while IFS= read -d '' OFILE; do
		#TODO VK_LIB ?
		TESTNAME="${OFILE%.*}"
		echo running test $TESTNAME
		test -f "${TESTNAME}" && cp "${TESTNAME}" "${TESTNAME}.bak.$(date '+%y%m%d%H%M')"
		$WORKNICE $COMPILER "$OFILE" -o "$TESTNAME" -L "${OUTDIR}" "-Wl,-rpath,$OUTDIR" $BUILTLIBS $LINKOPTS $BOTHOPTS 2>>"${ERRLOG}" >>"${LOGFILE}"
		echo running test "${TESTNAME}" >>"${LOGFILE}"
		$WORKNICE time $TESTNAME 2>>"${ERRLOG}" >>"${LOGFILE}"
		code=$?
		if [ $code -ne 0 ]; then echo "Exit code: " $code >>"${ERRLOG}"; fi;
		test -f "${TESTNAME}.bak" && ! test -f "${TESTNAME}" && cp "${TESTNAME}.bak" "${TESTNAME}"
	    done
    done
fi

#apps
if ! [ -f "${ERRLOG}" ] || [ "$(stat -c %s "${ERRLOG}")" -eq 0 ]; then
    for DIRNAME in $BUILDAPP; do
	find "$DIRNAME" -iname '*.o' | grep -qF .o || continue;
	APPNAME="$DIRNAME/$(basename "${DIRNAME}").so"
	#TODO VK_LIB ?
	echo "Linking $APPNAME"
	$WORKNICE $COMPILER $(find "$DIRNAME" -name '*.o') -o $APPNAME $BOTHOPTS >>"${LOGFILE}" 2>>"${ERRLOG}"
    done
fi

pkill $COMPILER

if [ -f "${ERRLOG}" ] && [ "$(stat -c %s "${ERRLOG}")" -gt 0 ]; then
    echo "${ERRLOG}"
    let i=0; while [ $i -lt 10 ]; do echo; let i++; done;
    head -n 40 "${ERRLOG}"
else
    echo "Success"
    tail "${LOGFILE}"
fi

