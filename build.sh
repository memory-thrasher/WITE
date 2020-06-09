BASEDIR="$(cd "$(dirname "$0")"; pwd -L)"
OUTDIR="${BASEDIR}/build/WITE"
LOGFILE="${OUTDIR}/buildlog.txt"
ERRLOG="${OUTDIR}/builderrs.txt"
rm "${LOGFILE}" "${ERRLOG}" 2>/dev/null
if [ "${1}" == "clean" ]; then
    rm -rf "${OUTDIR}"
fi
mkdir -p "${OUTDIR}" 2>/dev/null
cd "${BASEDIR}/WaffleIronTransactionalEngine"
if [ -z "${VK_SDK_PATH}" ]; then
    if [ -z "${VULKAN_SDK}" ]; then
	echo "Couldn't find vulkan sdk, please set VK_SDK_PATH environment variable."
	exit 1;
    fi
    VK_SDK_PATH="${VULKAN_SDK}"
fi
VK_INCLUDE="-I${VK_SDK_PATH}/Include -I${VK_SDK_PATH}/Third-Party/Include"
VK_LIB="-L${VK_SDK_PATH}/Lib32 -lvulkan-1"
let success=1;
find *.cpp -type f -print0 |
    while IFS= read -d '' SRCFILE; do
	DSTFILE="${OUTDIR}/${SRCFILE%.*}.o"
	DEPENDENCIES="${OUTDIR}/${SRCFILE%.*}.d"
	test -f "${DEPENDENCIES}" && test "${DEPENDENCIES}" -nt "${SRCFILE}" ||
	    g++ $VK_INCLUDE -E -o /dev/null -H "${SRCFILE}" 2>&1 | grep -o '[^[:space:]]*$' >"${DEPENDENCIES}"
	while read depend; do
	    if [ "${depend}" -nt "${DSTFILE}" ]; then
		echo "rebuilding because ${depend} is newer";
		break;
	    fi
	done <"${DEPENDENCIES}" | grep -F rebuild || continue;
	g++ $VK_INCLUDE -E -o /dev/null -H "${SRCFILE}" 2>&1 | grep -o '[^[:space:]]*$' >"${DEPENDENCIES}"
	g++ $VK_INCLUDE --std=c++17 -Werror -Wall "${SRCFILE}" -c -o "${DSTFILE}" >>"${LOGFILE}" 2>>"${ERRLOG}" || { let success=0; break; };
	echo "Built: ${SRCFILE}"
    done
if [ $success -eq 1 ]; then
    g++ $VK_LIB --std=c++17 -Werror -Wall -o "${OUTDIR}/WITE" $(find "${OUTDIR}" -type f -iname '*.o') >>"${LOGFILE}" 2>>"${ERRLOG}"
fi
if [ $(stat -c '%s' "${ERRLOG}") -gt 0 ]; then
    echo "${ERRLOG}"
    less -X -F "${ERRLOG}"
fi

