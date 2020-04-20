SET(CMAKE_C_COMPILER		/opt/nec/ve/bin/ncc)
SET(CMAKE_CXX_COMPILER		/opt/nec/ve/bin/nc++)
SET_DIRECTORY_PROPERTIES	(PROPERTIES COMPILE_OPTIONS "")
ADD_COMPILE_OPTIONS			(-fdiag-inline=0 -fdiag-parallel=0 -fdiag-vector=0 -O4)

ADD_LIBRARY				(urpcVE SHARED ${VE_SRC})
TARGET_LINK_LIBRARIES	(urpcVE veio veftrace pthread)

ADD_LIBRARY				(urpcVE_static STATIC ${VE_SRC})
SET_TARGET_PROPERTIES	(urpcVE_static PROPERTIES OUTPUT_NAME urpcVE)

ADD_LIBRARY				(urpcVE_omp SHARED ${VE_SRC})
TARGET_COMPILE_OPTIONS	(urpcVE_omp PRIVATE -fopenmp)
TARGET_LINK_OPTIONS		(urpcVE_omp PRIVATE -fopenmp)
TARGET_LINK_LIBRARIES	(urpcVE_omp veio veftrace)

ADD_LIBRARY				(urpcVE_omp_static STATIC ${VE_SRC})
TARGET_COMPILE_OPTIONS	(urpcVE_omp_static PRIVATE -fopenmp)
SET_TARGET_PROPERTIES	(urpcVE_omp_static PROPERTIES OUTPUT_NAME urpcVE_omp)

INSTALL(TARGETS urpcVE urpcVE_static urpcVE_omp urpcVE_omp_static LIBRARY DESTINATION lib)