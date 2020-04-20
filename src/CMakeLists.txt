## Define Source Code ---------------------------------------------------------
SET(VH_SRC
	${CMAKE_CURRENT_LIST_DIR}/init_hook.c
	${CMAKE_CURRENT_LIST_DIR}/vh_shm.c
	${CMAKE_CURRENT_LIST_DIR}/vh_urpc.c
	${CMAKE_CURRENT_LIST_DIR}/urpc_common.c
	${CMAKE_CURRENT_LIST_DIR}/memory.c
)

SET(VE_SRC
	${CMAKE_CURRENT_LIST_DIR}/init_hook.c
	${CMAKE_CURRENT_LIST_DIR}/ve_urpc.c
	${CMAKE_CURRENT_LIST_DIR}/urpc_common.c
	${CMAKE_CURRENT_LIST_DIR}/memory.c
)

SET(HEADER
	${CMAKE_CURRENT_LIST_DIR}/urpc.h
	${CMAKE_CURRENT_LIST_DIR}/urpc_debug.h
	${CMAKE_CURRENT_LIST_DIR}/urpc_time.h
)

## Build host libraries -------------------------------------------------------
ADD_DEFINITIONS(-DSYNCDMA -D_SVID_SOURCE)
ADD_LIBRARY(urpcVH SHARED ${VH_SRC})
ADD_LIBRARY(urpcVH_static STATIC ${VH_SRC})
SET_TARGET_PROPERTIES(urpcVH_static PROPERTIES OUTPUT_NAME urpcVH)

## Build device libraries -----------------------------------------------------
ADD_SUBDIRECTORY(ve)

## Install --------------------------------------------------------------------
INSTALL(TARGETS urpcVH urpcVH_static LIBRARY DESTINATION lib)
INSTALL(FILES ${HEADER} DESTINATION include)
