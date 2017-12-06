set(USR_LOCAL
  /gpfs/gpfs0/groups/mozafari/jiamin/.local/)

set(REPLAYER_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/sql_replayer.cc)

add_executable(sql_replayer
  ${REPLAYER_SRC})
target_include_directories(sql_replayer PRIVATE
  ${USR_LOCAL}/include)
target_link_libraries(sql_replayer
  ${USR_LOCAL}/lib64/libmysqlcppconn.so)
install(TARGETS sql_replayer RUNTIME DESTINATION bin)
