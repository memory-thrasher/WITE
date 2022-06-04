BASEDIR="$(cd "$(dirname "$0")"; pwd -L)"
OUTDIR="${BASEDIR}/build/WITE"
LOGFILE="${OUTDIR}/buildlog.txt"
ERRLOG="${OUTDIR}/builderrs.txt"
rm "${LOGFILE}" "${ERRLOG}" 2>/dev/null
mkdir -p "${OUTDIR}" 2>/dev/null
cd "${BASEDIR}/WITE"
if [ -z "${VK_SDK_PATH}" ]; then
    VK_SDK_PATH="${VULKAN_SDK}"
fi
COMPILER=clang++
if [ -z "$VK_INCLUDE" -a ! -z "$VK_SDK_PATH" ]; then
    VK_INCLUDE="-I${VK_SDK_PATH}/Include -I${VK_SDK_PATH}/Third-Party/Include"
fi
find -name '*.cpp' -type f -print0 |
    while IFS= read -d '' SRCFILE; do
	DSTFILE="${OUTDIR}/${SRCFILE%.*}.o"
	DEPENDENCIES="${OUTDIR}/${SRCFILE%.*}.d"
	if ! test -f "${DEPENDENCIES}" || test "${DEPENDENCIES}" -nt "${SRCFILE}"; then
	    $COMPILER $VK_INCLUDE -E -o /dev/null -H "${SRCFILE}" 2>&1 | grep -o '[^[:space:]]*$' >"${DEPENDENCIES}"
	fi
	while read depend; do
	    if [ "${depend}" -nt "${DSTFILE}" ]; then
		echo "rebuilding ${SRCFILE} because ${depend} is newer";
		break;
	    fi
	done <"${DEPENDENCIES}" | grep -F rebuild || continue;
	rm "${DEPENDENCIES}"
	$COMPILER $VK_INCLUDE --std=c++20 -Werror -Wall "${SRCFILE}" -c -o "${DSTFILE}" -DDEBUG >>"${LOGFILE}" 2>>"${ERRLOG}" || break;
	echo "Built: ${SRCFILE}"
    done
#TODO build .so and executables, one per top level dir
#if [ $(find -name '*.cpp' | wc -l) -eq $(find -name '*.o' | wc -l) ]; then
#    echo "All objects built, now linking"
#    $COMPILER $(find -name '*.o')
#fi
echo "${ERRLOG}"
if [ $(stat -c %s "${ERRLOG}") -gt 0 ]; then
    let i=0; while [ $i -lt 10 ]; do echo; let i++; done;
    head -n 60 "${ERRLOG}"
fi


