set(USR_LOCAL
  /users/POTaDOS/SQP/.local)

find_package (Threads)

set(REPLAYER_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/query_parser.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sql_replayer.cc)
add_executable(sql_replayer
  ${REPLAYER_SRC})
target_include_directories(sql_replayer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${USR_LOCAL}/include
  ${USR_LOCAL}/mysql/include)
target_link_libraries(sql_replayer PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  ${USR_LOCAL}/lib/libmysqlcppconn.so
  ${USR_LOCAL}/mysql/lib/libmysqlclient.so)
install(TARGETS sql_replayer RUNTIME DESTINATION bin)

set(SYNTHETIC_REPLAYER_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/query_parser.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/synthetic_sql_replayer.cc)
add_executable(synthetic_sql_replayer
  ${SYNTHETIC_REPLAYER_SRC})
target_include_directories(synthetic_sql_replayer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${USR_LOCAL}/include
  ${USR_LOCAL}/mysql/include)
target_link_libraries(synthetic_sql_replayer PRIVATE
  ${USR_LOCAL}/lib/libmysqlcppconn.so
  ${USR_LOCAL}/mysql/lib/libmysqlclient.so)
install(TARGETS synthetic_sql_replayer RUNTIME DESTINATION bin)
