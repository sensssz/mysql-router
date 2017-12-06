set(USR_LOCAL
  /gpfs/gpfs0/groups/mozafari/jiamin/.local/)

set(REPLAYER_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/sql_player.cc)

add_executable(sql_player
  ${REPLAYER_SRC})
target_include_directories(sql_replayer
  ${USR_LOCAL}/include)
target_link_libraries(sql_player
  ${USR_LOCAL}/lib64/libmysqlcppconn.so)
install(TARGETS sql_player RUNTIME DESTINATION bin)
