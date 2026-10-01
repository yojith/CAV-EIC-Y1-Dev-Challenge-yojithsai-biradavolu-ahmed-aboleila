# CMake generated Testfile for 
# Source directory: C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge
# Build directory: C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/build-v2-compare
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test("antworld_framework_tests" "C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/build-v2-compare/Debug/antworld_tests.exe")
  set_tests_properties("antworld_framework_tests" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/CMakeLists.txt;49;add_test;C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test("antworld_framework_tests" "C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/build-v2-compare/Release/antworld_tests.exe")
  set_tests_properties("antworld_framework_tests" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/CMakeLists.txt;49;add_test;C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test("antworld_framework_tests" "C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/build-v2-compare/MinSizeRel/antworld_tests.exe")
  set_tests_properties("antworld_framework_tests" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/CMakeLists.txt;49;add_test;C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test("antworld_framework_tests" "C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/build-v2-compare/RelWithDebInfo/antworld_tests.exe")
  set_tests_properties("antworld_framework_tests" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/CMakeLists.txt;49;add_test;C:/Users/yojit/Documents/Yojith_Work/Mac_EcoCar/CAV-EIC-Y1-Dev-Challenge/CMakeLists.txt;0;")
else()
  add_test("antworld_framework_tests" NOT_AVAILABLE)
endif()
