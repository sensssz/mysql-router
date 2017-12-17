set(USR_LOCAL
  /users/POTaDOS/.local)

set(REPLAYER_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sql_replayer.cc)

add_executable(sql_replayer
  ${REPLAYER_SRC})
target_include_directories(sql_replayer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${USR_LOCAL}/include
  ${USR_LOCAL}/mysql/include)
target_link_libraries(sql_replayer PRIVATE
  /usr/lib/libmysqlcppconn.so
  ${USR_LOCAL}/mysql/lib/libmysqlclient.so)
install(TARGETS sql_replayer RUNTIME DESTINATION bin)
