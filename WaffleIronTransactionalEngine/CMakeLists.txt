set (WITE_OBJS "")
file(GLOB WITE_CPPS RELATIVE "${CMAKE_SOURCE_DIR}/WaffleIronTransactionalEngine" "*.cpp")
foreach(SFILE ${WITE_CPPS})
	set (WITE_OBJS ${WITE_OBJS} ${SFILE})
endforeach(SFILE)

add_library( WITE ${WITE_OBJS} )
target_link_libraries(WITE ${SDL2_LIBRARIES})
