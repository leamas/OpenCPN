# ************************************************************************
#  Copyright (C) 2023 Alec Leamas                                        *
#  Copyright (C) 2010 David S Register                                   *
#                                                                        *
#  This program is free software; you can redistribute it and/or modify  *
#  it under the terms of the GNU General Public License as published by  *
#  the Free Software Foundation; either version 2 of the License, or     *
#  (at your option) any later version.                                   *
#                                                                        .
#  This program is distributed in the hope that it will be useful,       *
#  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#  GNU General Public License for more details.                          *
#                                                                        *
#  You should have received a copy of the GNU General Public License     *
#  along with this program; if not, write to the                         *
#  Free Software Foundation, Inc.,                                       *
#  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
# ************************************************************************


set(SRC_CONSOLE console.cpp api_shim.cpp)
add_executable(opencpn-cmd ${SRC_CONSOLE})
set_property(TARGET opencpn-cmd PROPERTY ENABLE_EXPORTS ON)
if (NOT "${ENABLE_SANITIZER}" MATCHES "none")
  target_link_libraries(opencpn-cmd PRIVATE -fsanitize=${ENABLE_SANITIZER})
endif ()

target_include_directories(
  opencpn-cmd
    PRIVATE
      ${PROJECT_SOURCE_DIR}/include
      # ${PROJECT_SOURCE_DIR}/test/include
      #      ${PROJECT_SOURCE_DIR}/model/include
      ${PROJECT_SOURCE_DIR}/gui/include
)

target_compile_definitions(opencpn-cmd PUBLIC USE_MOCK_DEFS)
if (MSVC)
  target_link_libraries(opencpn-cmd PRIVATE setupapi.lib psapi.lib)
endif ()

if (TARGET ocpn::wxcurl)
  target_link_libraries(opencpn-cmd PRIVATE ocpn::wxcurl)
else ()
  target_link_libraries(opencpn-cmd PRIVATE ${SYS_WXCURL_LIBRARIES})
endif ()

target_link_libraries(opencpn-cmd PRIVATE ${wxWidgets_LIBRARIES})
if (TARGET ocpn::wxsvg)
  target_link_libraries(opencpn-cmd PRIVATE ocpn::wxsvg)
endif ()


target_link_libraries(opencpn-cmd PRIVATE ocpn::serial)
target_link_libraries(opencpn-cmd PRIVATE ocpn::tinyxml)
target_link_libraries(opencpn-cmd PRIVATE ocpn::filesystem)

if (HAVE_LIBUDEV)
  target_link_libraries(opencpn-cmd PRIVATE ocpn::libudev)
endif ()

if(APPLE AND OCPN_USE_DEPS_BUNDLE)
  target_include_directories(
    opencpn-cmd PUBLIC ${OCPN_DEPS_BUNDLE_PATH}/include
  )
endif()

target_link_libraries(opencpn-cmd PRIVATE ocpn::model ocpn::model-src)
if (MSVC)
  target_link_libraries(opencpn-cmd PRIVATE  iphlpapi)
endif ()


if (WIN32)
  install(TARGETS opencpn-cmd RUNTIME DESTINATION ".")
else()
  install(TARGETS opencpn-cmd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

if (APPLE)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/opencpn-cmd
    DESTINATION "bin/OpenCPN.app/Contents/MacOS"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
      WORLD_EXECUTE GROUP_EXECUTE OWNER_EXECUTE
  )
  set_target_properties(opencpn-cmd PROPERTIES
                               XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
  )
endif ()
