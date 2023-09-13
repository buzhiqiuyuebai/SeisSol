option(HDF5 "Use HDF5 library for data output" ON)
option(NETCDF "Use netcdf library for mesh input" ON)

set(GRAPH_PARTITIONING_LIBS "parmetis" CACHE STRING "Graph partitioning library for mesh partitioning")
set(GRAPH_PARTITIONING_LIB_OPTIONS parmetis parhip ptscotch)
set_property(CACHE GRAPH_PARTITIONING_LIBS PROPERTY STRINGS ${GRAPH_PARTITIONING_LIB_OPTIONS})

option(MPI "Use MPI parallelization" ON)
option(MINI_SEISSOL "Use MiniSeisSol to compute node weights for load balancing" ON)
option(OPENMP "Use OpenMP parallelization" ON)
option(ASAGI "Use asagi for material input" OFF)
option(MEMKIND "Use memkind library for hbw memory support" OFF)
option(USE_IMPALA_JIT_LLVM "Use llvm version of impalajit" OFF)
option(LIKWID "Link with the likwid marker interface for proxy" OFF)
option(TARGETDART "Enable support for the targetDART runtime" ON) # TODO(David): if ever merged to a production branch, switch to OFF

option(INTEGRATE_QUANTITIES "Compute cell-averaged integrated velocity and stress components (currently breaks compilation)" OFF)
option(ADDRESS_SANITIZER_DEBUG "Use address sanitzer in debug mode" OFF)

option(TESTING "Compile unit tests" OFF)
option(TESTING_GENERATED "Include kernel tests generated by yateto" OFF)
option(COVERAGE "Generate targed for code coverage using lcob" OFF)

#Seissol specific
set(ORDER 6 CACHE STRING "Convergence order")  # must be INT type, by cmake-3.16 accepts only STRING
set(ORDER_OPTIONS 2 3 4 5 6 7 8)
set_property(CACHE ORDER PROPERTY STRINGS ${ORDER_OPTIONS})

set(NUMBER_OF_MECHANISMS 0 CACHE STRING "Number of mechanisms")

set(EQUATIONS "elastic" CACHE STRING "Equation set used")
set(EQUATIONS_OPTIONS elastic anisotropic viscoelastic viscoelastic2 poroelastic)
set_property(CACHE EQUATIONS PROPERTY STRINGS ${EQUATIONS_OPTIONS})


set(HOST_ARCH "hsw" CACHE STRING "Type of host architecture")
set(HOST_ARCH_OPTIONS noarch wsm snb hsw knc knl skx rome thunderx2t99 power9 a64fx)
# size of a vector registers in bytes for a given architecture
set(HOST_ARCH_ALIGNMENT   16  16  32  32  64  64  64   32       16     16     256)
set_property(CACHE HOST_ARCH PROPERTY STRINGS ${HOST_ARCH_OPTIONS})


set(DEVICE_BACKEND "none" CACHE STRING "Type of GPU backend")
set(DEVICE_BACKEND_OPTIONS none cuda hip hipsycl oneapi)
set_property(CACHE DEVICE_BACKEND PROPERTY STRINGS ${DEVICE_BACKEND_OPTIONS})


set(DEVICE_ARCH "none" CACHE STRING "Type of GPU architecture")
set(DEVICE_ARCH_OPTIONS none sm_60 sm_61 sm_62 sm_70 sm_71 sm_75 sm_80 sm_86 sm_90
        gfx906 gfx908 gfx90a
        dg1 bdw skl Gen8 Gen9 Gen11 Gen12LP)
set_property(CACHE DEVICE_ARCH PROPERTY STRINGS ${DEVICE_ARCH_OPTIONS})

set(PRECISION "double" CACHE STRING "type of floating point precision, namely: double/single")
set(PRECISION_OPTIONS single double)
set_property(CACHE PRECISION PROPERTY STRINGS ${PRECISION_OPTIONS})


set(PLASTICITY_METHOD "nb" CACHE STRING "Plasticity method: nb (nodal basis) is faster, ip (interpolation points) possibly more accurate. Recommended: nb")
set(PLASTICITY_OPTIONS nb ip)
set_property(CACHE PLASTICITY_METHOD PROPERTY STRINGS ${PLASTICITY_OPTIONS})


set(DR_QUAD_RULE "stroud" CACHE STRING "Dynamic Rupture quadrature rule")
set(DR_QUAD_RULE_OPTIONS stroud dunavant)
set_property(CACHE DR_QUAD_RULE PROPERTY STRINGS ${DR_QUAD_RULE_OPTIONS})


set(NUMBER_OF_FUSED_SIMULATIONS 1 CACHE STRING "A number of fused simulations")


set(MEMORY_LAYOUT "auto" CACHE FILEPATH "A file with a specific memory layout or auto")

option(COMMTHREAD "Use a communication thread for MPI+MP." OFF)

option(NUMA_AWARE_PINNING "Use libnuma to pin threads to correct NUMA nodes" ON)

option(PROXY_PYBINDING "enable pybind11 for proxy (everything will be compiled with -fPIC)" OFF)

set(LOG_LEVEL "warning" CACHE STRING "Log level for the code")
set(LOG_LEVEL_OPTIONS "debug" "info" "warning" "error")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS ${LOG_LEVEL_OPTIONS})

set(LOG_LEVEL_MASTER "info" CACHE STRING "Log level for the code")
set(LOG_LEVEL_MASTER_OPTIONS "debug" "info" "warning" "error")
set_property(CACHE LOG_LEVEL_MASTER PROPERTY STRINGS ${LOG_LEVEL_MASTER_OPTIONS})


set(GEMM_TOOLS_LIST "auto" CACHE STRING "choose a gemm tool(s) for the code generator")
set(GEMM_TOOLS_OPTIONS "auto" "LIBXSMM,PSpaMM" "LIBXSMM" "MKL" "OpenBLAS" "BLIS" "PSpaMM" "Eigen" "LIBXSMM,PSpaMM,GemmForge" "Eigen,GemmForge"
        "LIBXSMM_JIT,PSpaMM" "LIBXSMM_JIT" "LIBXSMM_JIT,PSpaMM,GemmForge")
set_property(CACHE GEMM_TOOLS_LIST PROPERTY STRINGS ${GEMM_TOOLS_OPTIONS})

#-------------------------------------------------------------------------------
# ------------------------------- ERROR CHECKING -------------------------------
#-------------------------------------------------------------------------------
function(check_parameter parameter_name value options)

    list(FIND options ${value} INDEX)

    set(WRONG_PARAMETER -1)
    if (${INDEX} EQUAL ${WRONG_PARAMETER})
        message(FATAL_ERROR "${parameter_name} is wrong. Specified \"${value}\". Allowed: ${options}")
    endif()

endfunction()


check_parameter("ORDER" ${ORDER} "${ORDER_OPTIONS}")
check_parameter("HOST_ARCH" ${HOST_ARCH} "${HOST_ARCH_OPTIONS}")
check_parameter("DEVICE_BACKEND" ${DEVICE_BACKEND} "${DEVICE_BACKEND_OPTIONS}")
check_parameter("DEVICE_ARCH" ${DEVICE_ARCH} "${DEVICE_ARCH_OPTIONS}")
check_parameter("EQUATIONS" ${EQUATIONS} "${EQUATIONS_OPTIONS}")
check_parameter("PRECISION" ${PRECISION} "${PRECISION_OPTIONS}")
check_parameter("PLASTICITY_METHOD" ${PLASTICITY_METHOD} "${PLASTICITY_OPTIONS}")
check_parameter("LOG_LEVEL" ${LOG_LEVEL} "${LOG_LEVEL_OPTIONS}")
check_parameter("LOG_LEVEL_MASTER" ${LOG_LEVEL_MASTER} "${LOG_LEVEL_MASTER_OPTIONS}")

# deduce GEMM_TOOLS_LIST based on the host arch
if (GEMM_TOOLS_LIST STREQUAL "auto")
    set(X86_ARCHS wsm snb hsw knc knl skx)
    set(WITH_AVX512_SUPPORT knl skx)

    if (${HOST_ARCH} IN_LIST X86_ARCHS)
        if (${HOST_ARCH} IN_LIST WITH_AVX512_SUPPORT)
            set(GEMM_TOOLS_LIST "LIBXSMM,PSpaMM")
        else()
            set(GEMM_TOOLS_LIST "LIBXSMM")
        endif()
    else()
        set(GEMM_TOOLS_LIST "Eigen")
    endif()
endif()

if (NOT ${DEVICE_BACKEND} STREQUAL "none")
    set(GEMM_TOOLS_LIST "${GEMM_TOOLS_LIST},GemmForge")
    set(WITH_GPU on)
else()
    set(WITH_GPU off)
endif()
message(STATUS "GEMM TOOLS are: ${GEMM_TOOLS_LIST}")


if (DEVICE_ARCH MATCHES "sm_*")
    set(DEVICE_VENDOR "nvidia")
    set(PREMULTIPLY_FLUX_DEFAULT ON)
elseif(DEVICE_ARCH MATCHES "gfx*")
    set(DEVICE_VENDOR "amd")
    set(PREMULTIPLY_FLUX_DEFAULT ON)
else()
    # TODO(David): adjust as soon as we add support for more vendors
    set(DEVICE_VENDOR "intel")
    set(PREMULTIPLY_FLUX_DEFAULT OFF)
endif()

if (WITH_GPU)
    option(PREMULTIPLY_FLUX "Merge device flux matrices (recommended for AMD and Nvidia GPUs)" ${PREMULTIPLY_FLUX_DEFAULT})
endif()

# check compute sub architecture (relevant only for GPU)
if (NOT ${DEVICE_ARCH} STREQUAL "none")
    if (${DEVICE_BACKEND} STREQUAL "none")
        message(FATAL_ERROR "DEVICE_BACKEND is not provided for ${DEVICE_ARCH}")
    endif()

    if (${DEVICE_ARCH} MATCHES "sm_*")
        set(ALIGNMENT  64)
    elseif(${DEVICE_ARCH} MATCHES "gfx*")
        set(ALIGNMENT  128)
    else()
        set(ALIGNMENT 128)
        message(STATUS "Assume ALIGNMENT = 32, for DEVICE_ARCH=${DEVICE_ARCH}")
    endif()
else()
    list(FIND HOST_ARCH_OPTIONS ${HOST_ARCH} INDEX)
    list(GET HOST_ARCH_ALIGNMENT ${INDEX} ALIGNMENT)
    set(DEVICE_BACKEND "none")
endif()

# check NUMBER_OF_MECHANISMS
if ((NOT "${EQUATIONS}" MATCHES "viscoelastic.?") AND ${NUMBER_OF_MECHANISMS} GREATER 0)
    message(FATAL_ERROR "${EQUATIONS} does not support a NUMBER_OF_MECHANISMS > 0.")
endif()

if ("${EQUATIONS}" MATCHES "viscoelastic.?" AND ${NUMBER_OF_MECHANISMS} LESS 1)
    message(FATAL_ERROR "${EQUATIONS} needs a NUMBER_OF_MECHANISMS > 0.")
endif()


# derive a byte representation of real numbers
if ("${PRECISION}" STREQUAL "double")
    set(REAL_SIZE_IN_BYTES 8)
elseif ("${PRECISION}" STREQUAL "single")
    set(REAL_SIZE_IN_BYTES 4)
endif()


# check NUMBER_OF_FUSED_SIMULATIONS
math(EXPR IS_ALIGNED_MULT_SIMULATIONS 
        "${NUMBER_OF_FUSED_SIMULATIONS} % (${ALIGNMENT} / ${REAL_SIZE_IN_BYTES})")

if (NOT ${NUMBER_OF_FUSED_SIMULATIONS} EQUAL 1 AND NOT ${IS_ALIGNED_MULT_SIMULATIONS} EQUAL 0)
    math(EXPR FACTOR "${ALIGNMENT} / ${REAL_SIZE_IN_BYTES}")
    message(FATAL_ERROR "a number of fused must be multiple of ${FACTOR}")
endif()

#-------------------------------------------------------------------------------
# -------------------- COMPUTE/ADJUST ADDITIONAL PARAMETERS --------------------
#-------------------------------------------------------------------------------
# PDE-Settings
if (EQUATIONS STREQUAL "poroelastic")
  set(NUMBER_OF_QUANTITIES "13")
else()
  MATH(EXPR NUMBER_OF_QUANTITIES "9 + 6 * ${NUMBER_OF_MECHANISMS}" )
endif()

# generate an internal representation of an architecture type which is used in seissol
string(SUBSTRING ${PRECISION} 0 1 PRECISION_PREFIX)
if (${PRECISION} STREQUAL "double")
    set(HOST_ARCH_STR "d${HOST_ARCH}")
    set(DEVICE_ARCH_STR "d${DEVICE_ARCH}")
elseif(${PRECISION} STREQUAL "single")
    set(HOST_ARCH_STR "s${HOST_ARCH}")
    set(DEVICE_ARCH_STR "s${DEVICE_ARCH}")
endif()

if (${DEVICE_ARCH} STREQUAL "none")
    set(DEVICE_ARCH_STR "none")
endif()


function(cast_log_level_to_int log_level_str log_level_int)
  if (${log_level_str} STREQUAL "debug")
    set(${log_level_int} 3 PARENT_SCOPE)
  elseif (${log_level_str} STREQUAL "info")
    set(${log_level_int} 2 PARENT_SCOPE)
  elseif (${log_level_str} STREQUAL "warning")
    set(${log_level_int} 1 PARENT_SCOPE)
  elseif (${log_level_str} STREQUAL "error")
    set(${log_level_int} 0 PARENT_SCOPE)
  endif()
endfunction()

cast_log_level_to_int(LOG_LEVEL LOG_LEVEL)
cast_log_level_to_int(LOG_LEVEL_MASTER LOG_LEVEL_MASTER)

if (PROXY_PYBINDING)
    set(EXTRA_CXX_FLAGS -fPIC)

    # Note: ENABLE_PIC_COMPILATION can be used to signal other sub-modules
    # generate position independent code
    set(ENABLE_PIC_COMPILATION ON)
endif()
