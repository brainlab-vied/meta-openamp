PROVIDES:arm = " open-amp open-amp-xlnx "
RPROVIDES:${PN} += "open-amp"
RPROVIDES:${PN}-demos += "open-amp-demos"

inherit ccmake cmake python3-dir deploy

S = "${WORKDIR}/git"
B = "${WORKDIR}/build"

OECMAKE_SOURCEPATH = "${S}/"

DEPENDS += " python3-pyyaml-native lopper-native python3-dtc-native "
DEPENDS_BAREMETAL:linux = ""
DEPENDS_BAREMETAL:armv7r = "xiltimer libxil xilstandalone xiltimer "
DEPENDS_BAREMETAL:armv8r = "xiltimer libxil xilstandalone xiltimer "

DEPENDS += "${DEPENDS_BAREMETAL} "
DEPENDS_FREERTOS ??= ""
DEPENDS_FREERTOS:xilinx-freertos = "freertos10-xilinx"

DEPENDS += "${DEPENDS_FREERTOS}"

OPENAMP_WITH_PROXY ?= "OFF"

OECMAKE_C_LINK_FLAGS:append:arm = " --sysroot=${STAGING_DIR_HOST}"
CFLAGS:append = " -DSDT -D_AMD_GENERATED_ -DVIRTIO_USE_DCACHE ${DEBUG_PREFIX_MAP} "
CFLAGS:append:armv7r = " -O3  -DXLNX_PLATFORM  -specs=${PKG_CONFIG_SYSROOT_DIR}/usr/include/Xilinx.spec "
CFLAGS:append:armv8r = " -O3  -DXLNX_PLATFORM  -specs=${PKG_CONFIG_SYSROOT_DIR}/usr/include/Xilinx.spec "
CFLAGS:append:xilinx-freertos = " -DUSE_FREERTOS "

OPENAMP_CMAKE_SOC = "unknown"
OPENAMP_CMAKE_SOC:zynqmp = "ZYNQMP"
OPENAMP_CMAKE_SOC:versal = "VERSAL"
OPENAMP_CMAKE_SOC:versal-net = "VERSAL_NET"

OPENAMP_CMAKE_SYSTEM_NAME = "unknown"
OPENAMP_CMAKE_SYSTEM_NAME:xilinx-standalone = "Generic"
OPENAMP_CMAKE_SYSTEM_NAME:xilinx-freertos = "FreeRTOS"

LINKER_METADATA_FILE="OpenAMP_Lopper_Linker_Cfg.cmake"

TOOLCHAIN_FILE_MACHINE:armv7r = "zynqmp_r5"
TOOLCHAIN_FILE_MACHINE:armv8r = "zynqmp_r5"
TOOLCHAIN_FILE_MACHINE:linux = "${OPENAMP_MACHINE}"

OPENAMP_MACHINE:xilinx-standalone = "${TOOLCHAIN_FILE_MACHINE}"
OPENAMP_MACHINE:xilinx-freertos = "${TOOLCHAIN_FILE_MACHINE}"

DEMO_LINK_FLAGS ?= ""
DEMO_LINK_FLAGS:append:armv7r = " -lxilstandalone -lscugic -lxiltimer"
DEMO_LINK_FLAGS:append:armv8r = " -lxilstandalone -lscugic -lxiltimer"
DEMO_LINK_FLAGS:append:xilinx-freertos = " -lfreertos "

# Generate Lopper linker config file before generating toolchain file
cmake_do_generate_toolchain_file:append:arm() {
    cat >> ${WORKDIR}/toolchain.cmake <<EOF
	include (CMakeForceCompiler)
	CMAKE_FORCE_C_COMPILER("${OECMAKE_C_COMPILER}" GNU)
	set (CMAKE_SYSTEM_PROCESSOR "${TRANSLATED_TARGET_ARCH}" )
	set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ")
	set (CMAKE_SYSTEM_NAME      "${OPENAMP_CMAKE_SYSTEM_NAME}")
	set (MACHINE                "${TOOLCHAIN_FILE_MACHINE}" )
	set (CMAKE_LIBRARY_PATH     "${PKG_CONFIG_SYSROOT_DIR}/usr/lib" CACHE STRING "")
	set (CMAKE_INCLUDE_PATH     "${CMAKE_INCLUDE_PATH} ${PKG_CONFIG_SYSROOT_DIR}/usr/include/" CACHE STRING "")
	set (CMAKE_FIND_ROOT_PATH   "${CMAKE_FIND_ROOT_PATH} ${STAGING_LIBDIR} ${CMAKE_INCLUDE_PATH} " CACHE STRING "")
	set (LIBMETAL_INCLUDE_DIR   " ${PKG_CONFIG_SYSROOT_DIR}/usr/include/" CACHE STRING "")
	set (LIBMETAL_LIB_DIR       " ${PKG_CONFIG_SYSROOT_DIR}/usr/lib" CACHE STRING "")
	set (XIL_INCLUDE_DIR        " ${PKG_CONFIG_SYSROOT_DIR}/usr/include/" CACHE STRING "")
	set (CMAKE_C_FLAGS          " ${CMAKE_C_FLAGS}  ${PKG_CONFIG_SYSROOT_DIR}/usr/include/" CACHE STRING "")
	set (WITH_PROXY             "${OPENAMP_WITH_PROXY}" CACHE STRING "")
	set_property (GLOBAL PROPERTY HAS_SYSTEM_DT ON)
	set_property(GLOBAL PROPERTY SOC "${OPENAMP_CMAKE_SOC}")
	set_property(GLOBAL PROPERTY LINKER_METADATA_FILE ${S}/${LINKER_METADATA_FILE})
	set (DEMO_LINK_FLAGS       "${DEMO_LINK_FLAGS}" CACHE STRING "")
	set (WITH_APPS              ON)
EOF
}

do_generate_toolchain_file[postfuncs] += "openamp_toolchain_file_setup"
LOPPER_GEN_FILE_OUTPUT:armv7r = "zynqmp_r5"
LOPPER_GEN_FILE_OUTPUT:armv8r = "zynqmp_r5"

OPENAMP_CFG_HEADER = "${CONFIG_DTFILE_DIR}/amd_platform_info.h"
OPENAMP_CFG_HEADER:linux = ""

openamp_toolchain_file_setup() {
  if [ -n "${OPENAMP_CFG_HEADER}" ]; then
    if [ ! -e "${OPENAMP_CFG_HEADER}" ]; then
       bberror "${OPENAMP_CFG_HEADER} is not present, we can't continue"
       exit 1
    fi
    cp ${OPENAMP_CFG_HEADER} ${S}/apps/machine/${LOPPER_GEN_FILE_OUTPUT}
  fi
}

python() {
    cfg_header = d.getVar('OPENAMP_CFG_HEADER')
    if cfg_header and not os.path.exists(cfg_header):
        raise bb.parse.SkipRecipe('Expected configuration file (%s) does not exist.' % (cfg_header))
}

FW_MACHINEzynqmp = "cortexr5-0"
FW_MACHINE:versal = "cortexr5-0"
FW_MACHINE:versal-net = "cortexr52-0"
FW_OS ?= "baremetal"
FW_TARGET ?= "${MACHINE}-${FW_MACHINE}-${FW_OS}"
LOPPER_INPUT_DT = "${CONFIG_DTFILE_DIR}/${FW_TARGET}.dts"

do_gen_linker() {
     export LOPPER_DTC_FLAGS="-b 0 -@"

     cd ${S}/
     lopper -O ${S} ${LOPPER_INPUT_DT} -- baremetallinker_xlnx ${ESW_MACHINE} ${S} openamp
     mv *Example.cmake ${LINKER_METADATA_FILE}
     cd -
}
do_gen_linker:linux() {
}

do_gen_linker[depends] = " lopper-native:do_populate_sysroot dtc-native:do_populate_sysroot "

do_compile_rpu() {
       cmake_runcmake_build --target rpmsg-echo.out matrix_multiplyd.out rpc_demo.out

       ${OBJCOPY} -O binary ${B}/apps/examples/echo/rpmsg-echo.out  ${B}/apps/examples/echo/rpmsg-echo.bin
       ${OBJCOPY} -O binary ${B}/apps/examples/matrix_multiply/matrix_multiplyd.out ${B}/apps/examples/matrix_multiply/matrix_multiplyd.bin
       ${OBJCOPY} -O binary ${B}/apps/examples/rpc_demo/rpc_demo.out ${B}/apps/examples/rpc_demo/rpc_demo.bin
}

do_install_rpu() {
	install -d ${D}/${bindir}
	install -m 0755 ${B}/apps/examples/echo/rpmsg-echo.out ${D}/${bindir}
	install -m 0755 ${B}/apps/examples/matrix_multiply/matrix_multiplyd.out ${D}/${bindir}
	install -m 0755 ${B}/apps/examples/rpc_demo/rpc_demo.out ${D}/${bindir}

	install -m 0755 ${B}/apps/examples/echo/rpmsg-echo.out ${D}/${bindir}/rpmsg-echo.elf
	install -m 0755 ${B}/apps/examples/matrix_multiply/matrix_multiplyd.out ${D}/${bindir}/matrix_multiplyd.elf
	install -m 0755 ${B}/apps/examples/rpc_demo/rpc_demo.out ${D}/${bindir}/rpc_demo.elf
}

PACKAGE_ARCH:linux = "${MACHINE_ARCH}"
COMPATIBLE_HOST:linux = ".*"
COMPATIBLE_HOST:arm = "[^-]*-[^-]*-eabi"

do_deploy() {
}
do_deploy_rpu() {
	install -d ${DEPLOYDIR}/
	install -Dm 0644 ${D}/${bindir}/*.out ${DEPLOYDIR}/
	install -Dm 0644 ${D}/${bindir}/*.elf ${DEPLOYDIR}/
	# below is needed for packaging

	install -m 0644 ${B}/apps/examples/echo/rpmsg-echo.bin ${DEPLOYDIR}/rpmsg-echo.bin
	install -m 0644 ${B}/apps/examples/matrix_multiply/matrix_multiplyd.bin ${DEPLOYDIR}/matrix_multiplyd.bin
	install -m 0644 ${B}/apps/examples/rpc_demo/rpc_demo.bin ${DEPLOYDIR}/rpc_demo.bin
}

do_compile:armv7r() {
	do_compile_rpu
}
do_compile:armv8r() {
	do_compile_rpu
}

do_install:armv7r() {
	do_install_rpu
}
do_install:armv8r() {
	do_install_rpu
}

do_deploy:armv7r() {
	do_deploy_rpu
}
do_deploy:armv8r() {
	do_deploy_rpu
}

addtask do_deploy before do_build after do_install
addtask do_gen_linker before do_configure
FILES:${PN}:append:armv7r = " ${libdir}/*.a ${bindir}/*.elf ${bindir}/*bin ${bindir}/*out "
FILES:${PN}:append:armv8r = " ${libdir}/*.a ${bindir}/*.elf ${bindir}/*bin ${bindir}/*out "

FILES:${PN}:append:linux = " ${DEPLOYDIR}${LOPPER_OPENAMP_OUT_DTB} "
