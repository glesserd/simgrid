set(example java_async)
set(sources ${CMAKE_CURRENT_SOURCE_DIR}/AsyncTest.java  ${CMAKE_CURRENT_SOURCE_DIR}/FinalizeTask.java
            ${CMAKE_CURRENT_SOURCE_DIR}/Forwarder.java  ${CMAKE_CURRENT_SOURCE_DIR}/Master.java
            ${CMAKE_CURRENT_SOURCE_DIR}/Slave.java)

if(enable_java)
  add_custom_command(
    COMMENT "Building ${example}..."
    OUTPUT ${example}_compiled
    DEPENDS ${sources} simgrid-java_jar ${SIMGRID_JAR}
    COMMAND ${JAVA_COMPILE} -classpath ${SIMGRID_JAR} -d ${CMAKE_CURRENT_BINARY_DIR}/.. ${sources}
    COMMAND ${CMAKE_COMMAND} -E remove ${example}_compiled
    COMMAND ${CMAKE_COMMAND} -E touch ${example}_compiled
  )
  add_custom_target(${example} ALL DEPENDS ${example}_compiled)
  ADD_TESH(java-async --setenv srcdir=${CMAKE_HOME_DIRECTORY}/examples/java --setenv classpath=${TESH_CLASSPATH} --cd ${CMAKE_BINARY_DIR}/examples/java ${CMAKE_HOME_DIRECTORY}/examples/java/async/async.tesh)
endif()


set(tesh_files    ${tesh_files}    ${CMAKE_CURRENT_SOURCE_DIR}/async.tesh           PARENT_SCOPE)
set(xml_files     ${xml_files}     ${CMAKE_CURRENT_SOURCE_DIR}/asyncDeployment.xml  PARENT_SCOPE)
set(examples_src  ${examples_src}  ${sources}                                       PARENT_SCOPE)
set(txt_files     ${txt_files}     ${CMAKE_CURRENT_SOURCE_DIR}/README               PARENT_SCOPE)
