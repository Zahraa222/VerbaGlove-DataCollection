idf_component_register(SRCS "" INCLUDE_DIRS "")

# Add all CSV and text files
file(GLOB LITTLEFS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.csv"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.txt"
)

foreach(file ${LITTLEFS_FILES})
    get_filename_component(file_name ${file} NAME)
    target_add_binary_data(${COMPONENT_TARGET} "${file}" TEXT)
endforeach()
