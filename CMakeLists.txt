cmake_minimum_required (VERSION 2.8)
project (ganalytics CXX)

find_package ( Qt4 COMPONENTS QtCore QtNetwork QtGui REQUIRED )

set (ganalytics_MOC_HDRS
  ganalytics.hpp
  )

set (ganalytics_SOURCES
  main.cpp 
  )

# enable warnings
add_definitions ( -Wall )

# add some useful macros and variables
# (QT_USE_FILE is a variable defined by FIND_PACKAGE( Qt4 ) that contains a path to CMake script)
include ( ${QT_USE_FILE} )

# and finally this will run moc:
qt4_wrap_cpp ( SAMPLE_MOC_SRCS ${ganalytics_MOC_HDRS} )

# we need this to be able to include headers produced by uic in our code
# (CMAKE_BINARY_DIR holds a path to the build directory, while 
#  INCLUDE_DIRECTORIES() works just like INCLUDEPATH from qmake)
include_directories ( ${CMAKE_BINARY_DIR} ${QWT_INCLUDE_DIR} )

# here we instruct CMake to build "sample" executable from all of the source files
add_executable ( ganalytics
  ${ganalytics_SOURCES}
  ${SAMPLE_MOC_SRCS} )

# last thing we have to do is to tell CMake what libraries our executable needs,
# luckily FIND_PACKAGE prepared QT_LIBRARIES variable for us:
target_link_libraries ( ganalytics ${QT_LIBRARIES} ${QWT_LIBRARIES} )
