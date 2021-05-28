BASEDIR="$(cd "$(dirname "$0")"; pwd -L)"
OUTDIR="${BASEDIR}/build/WITE"
LOGFILE="${OUTDIR}/buildlog.txt"
ERRLOG="${OUTDIR}/builderrs.txt"
rm "${LOGFILE}" "${ERRLOG}" 2>/dev/null
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
find -name *.cpp -type f -print0 |
    while IFS= read -d '' SRCFILE; do
	DSTFILE="${OUTDIR}/${SRCFILE%.*}.o"
	DEPENDENCIES="${OUTDIR}/${SRCFILE%.*}.d"
	test -f "${DEPENDENCIES}" -a "${DEPENDENCIES}" -nt "${SRCFILE}" ||
	    g++ $VK_INCLUDE -E -o /dev/null -H "${SRCFILE}" 2>&1 | grep -o '[^[:space:]]*$' >"${DEPENDENCIES}"
	while read depend; do
	    if [ "${depend}" -nt "${DSTFILE}" ]; then
		echo "rebuilding because ${depend} is newer";
		#break;
	    fi
	done <"${DEPENDENCIES}" | grep -F rebuild || continue;
	rm "${DEPENDENCIES}"
	g++ $VK_INCLUDE --std=c++17 -Werror -Wall "${SRCFILE}" -c -o "${DSTFILE}" >>"${LOGFILE}" 2>>"${ERRLOG}" || break;
	echo "Built: ${SRCFILE}"
    done
echo "${ERRLOG}"
less "${ERRLOG}"
