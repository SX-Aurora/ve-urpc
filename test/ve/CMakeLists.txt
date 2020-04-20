SET(CMAKE_C_COMPILER		/opt/nec/ve/bin/ncc)
SET(CMAKE_CXX_COMPILER		/opt/nec/ve/bin/nc++)
SET_DIRECTORY_PROPERTIES	(PROPERTIES COMPILE_OPTIONS "")
ADD_COMPILE_OPTIONS			(-fdiag-inline=0 -fdiag-parallel=0 -fdiag-vector=0 -O4 -no-proginf -no-perfcnt)

ADD_EXECUTABLE			(pong_ve ${VE_SRC})
TARGET_LINK_LIBRARIES	(pong_ve urpcVE veio pthread)
#INSTALL					(TARGETS pong_ve RUNTIME DESTINATION tests)