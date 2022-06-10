BUILDLIBS="WITE"
BUILDTESTS="Tests"
BUILDAPP=""
BASEDIR="$(cd "$(dirname "$0")"; pwd -L)"
OUTDIR="${BASEDIR}/build" #TODO not just WITE but all subdirs
LOGFILE="${OUTDIR}/buildlog.txt"
ERRLOG="${OUTDIR}/builderrs.txt"
rm "${LOGFILE}" "${ERRLOG}" 2>/dev/null
mkdir -p "${OUTDIR}" 2>/dev/null
if [ -z "${VK_SDK_PATH}" ]; then
    VK_SDK_PATH="${VULKAN_SDK}"
fi
COMPILER=clang++

#config
if [ -z "$VK_INCLUDE" -a ! -z "$VK_SDK_PATH" ]; then
    VK_INCLUDE="-I${VK_SDK_PATH}/Include -I${VK_SDK_PATH}/Third-Party/Include"
fi

#compile to static
find $BUILDLIBS $BUILDAPP $BUILDTESTS -name '*.cpp' -type f -print0 |
    while IFS= read -d '' SRCFILE; do
	DSTFILE="${OUTDIR}/${SRCFILE%.*}.o"
	DEPENDENCIES="${OUTDIR}/${SRCFILE%.*}.d"
	mkdir -p "$(dirname "$DSTFILE")";
	if [ -f "${DSTFILE}" ] && [ "${DSTFILE}" -nt "$0" ] && [ "${DSTFILE}" -nt "${SRCFILE}" ]; then
	    if ! test -f "${DEPENDENCIES}" || test "${SRCFILE}" -nt "${DEPENDENCIES}"; then
		$COMPILER $VK_INCLUDE -E -o /dev/null -H "${SRCFILE}" 2>&1 | grep -o '[^[:space:]]*$' >"${DEPENDENCIES}"
	    fi
	    while read depend; do
		if [ "${depend}" -nt "${DSTFILE}" ]; then
		    echo "rebuilding ${SRCFILE} because ${depend} is newer";
		    break;
		fi
	    done <"${DEPENDENCIES}" | grep -F rebuild || continue;
	else
	    # echo skipped new check for $DSTFILE
	    $COMPILER $VK_INCLUDE -E -o /dev/null -H "${SRCFILE}" 2>&1 | grep -o '[^[:space:]]*$' >"${DEPENDENCIES}"
	fi
	rm "${DEPENDENCIES}"
	if ! $COMPILER $VK_INCLUDE --std=c++20 -fPIC -Werror -Wall "${SRCFILE}" -c -o "${DSTFILE}" -DDEBUG >>"${LOGFILE}" 2>>"${ERRLOG}"; then
	    touch "${SRCFILE}";
	    break;
	fi
	echo "Built: ${SRCFILE}"
    done

#TODO other file types (shaders etc.)

#libs
for DIRNAME in $BUILDLIBS; do
    echo "Pre-Linking $DIRNAME"
    echo find "$DIRNAME" -iname '*.o'
    find "$DIRNAME" -iname '*.o'
    find "$DIRNAME" -iname '*.o' | grep -qF .o || continue;
    LIBNAME="$DIRNAME/$(basename "${DIRNAME}").so"
    if [ -f "$LIBNAME" ] && [ $(find "$DIRNAME" -iname '*.o' -newer "$LIBNAME" | wc -l) -eq 0 ]; then continue; fi;
    #TODO VK_LIB ?
    echo "Linking $LIBNAME"
    $COMPILER -shared $(find "$DIRNAME" -name '*.o') -o $LIBNAME >>"${LOGFILE}" 2>>"${ERRLOG}"
done
BUILTLIBS="$(find $OUTDIR -iname '*.so')"

#tests
for DIRNAME in $BUILDTESTS; do
    find "$OUTDIR/$DIRNAME" -iname '*.o' -print0 |
	while IFS= read -d '' OFILE; do
	    #TODO VK_LIB ?
	    TESTNAME="${OFILE%.*}"
	    echo "Building test $TESTNAME"
	    $COMPILER "$OFILE" -o "$TESTNAME" -L "${OUTDIR}" >>"${LOGFILE}" 2>>"${ERRLOG}"
	    echo running test "${TESTNAME}">>"${LOGFILE}"
	    $TESTNAME 2>>"${ERRLOG}" >>"${LOGFILE}"
	done
done

#apps
for DIRNAME in $BUILDAPP; do
    find "$DIRNAME" -iname '*.o' | grep -qF .o || continue;
    APPNAME="$DIRNAME/$(basename "${DIRNAME}").so"
    #TODO VK_LIB ?
    echo "Linking $APPNAME"
    $COMPILER $(find "$DIRNAME" -name '*.o') -o $APPNAME >>"${LOGFILE}" 2>>"${ERRLOG}"
done

if [ -f "${ERRLOG}" ] && [ $(stat -c %s "${ERRLOG}") -gt 0 ]; then
    echo "${ERRLOG}"
    let i=0; while [ $i -lt 10 ]; do echo; let i++; done;
    head -n 60 "${ERRLOG}"
else
    echo "Success"
fi


