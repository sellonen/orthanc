##
## This is a CMake configuration file that configures the core
## libraries of Orthanc. This file can be used by external projects so
## as to gain access to the Orthanc APIs (the most prominent examples
## are currently "Stone of Orthanc" and "Orthanc for whole-slide
## imaging plugin").
##


#####################################################################
## Configuration of the components
#####################################################################

# Path to the root folder of the Orthanc distribution
set(ORTHANC_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)

# Some basic inclusions
include(CMakePushCheckState)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(CheckTypeSize)
include(FindPythonInterp)
  
include(${CMAKE_CURRENT_LIST_DIR}/AutoGeneratedCode.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/DownloadPackage.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/Compiler.cmake)


#####################################################################
## Disable unneeded macros
#####################################################################

if (NOT ENABLE_SQLITE)
  unset(USE_SYSTEM_SQLITE CACHE)
  add_definitions(-DORTHANC_ENABLE_SQLITE=0)
endif()

if (NOT ENABLE_CRYPTO_OPTIONS)
  unset(ENABLE_SSL CACHE)
  unset(ENABLE_PKCS11 CACHE)
  unset(ENABLE_OPENSSL_ENGINES CACHE)
  unset(OPENSSL_STATIC_VERSION CACHE)
  unset(USE_SYSTEM_OPENSSL CACHE)
  unset(USE_SYSTEM_LIBP11 CACHE)
  add_definitions(
    -DORTHANC_ENABLE_SSL=0
    -DORTHANC_ENABLE_PKCS11=0
    )
endif()

if (NOT ENABLE_WEB_CLIENT)
  unset(USE_SYSTEM_CURL CACHE)
  add_definitions(-DORTHANC_ENABLE_CURL=0)
endif()

if (NOT ENABLE_WEB_SERVER)
  unset(ENABLE_CIVETWEB CACHE)
  unset(USE_SYSTEM_CIVETWEB CACHE)
  unset(USE_SYSTEM_MONGOOSE CACHE)
  add_definitions(
    -DORTHANC_ENABLE_CIVETWEB=0
    -DORTHANC_ENABLE_MONGOOSE=0
    )
endif()

if (NOT ENABLE_JPEG)
  unset(USE_SYSTEM_LIBJPEG CACHE)
  add_definitions(-DORTHANC_ENABLE_JPEG=0)
endif()

if (NOT ENABLE_ZLIB)
  unset(USE_SYSTEM_ZLIB CACHE)
  add_definitions(-DORTHANC_ENABLE_ZLIB=0)
endif()

if (NOT ENABLE_PNG)
  unset(USE_SYSTEM_LIBPNG CACHE)
  add_definitions(-DORTHANC_ENABLE_PNG=0)
endif()

if (NOT ENABLE_LUA)
  unset(USE_SYSTEM_LUA CACHE)
  unset(ENABLE_LUA_MODULES CACHE)
  add_definitions(-DORTHANC_ENABLE_LUA=0)
endif()

if (NOT ENABLE_PUGIXML)
  unset(USE_SYSTEM_PUGIXML CACHE)
  add_definitions(-DORTHANC_ENABLE_PUGIXML=0)
endif()

if (NOT ENABLE_LOCALE)
  unset(BOOST_LOCALE_BACKEND CACHE)
  add_definitions(-DORTHANC_ENABLE_LOCALE=0)
endif()

if (NOT ENABLE_GOOGLE_TEST)
  unset(USE_SYSTEM_GOOGLE_TEST CACHE)
  unset(USE_GOOGLE_TEST_DEBIAN_PACKAGE CACHE)
endif()

if (NOT ENABLE_DCMTK)
  add_definitions(
    -DORTHANC_ENABLE_DCMTK=0
    -DORTHANC_ENABLE_DCMTK_NETWORKING=0
    -DORTHANC_ENABLE_DCMTK_TRANSCODING=0
    )
  unset(DCMTK_DICTIONARY_DIR CACHE)
  unset(DCMTK_VERSION CACHE)
  unset(USE_DCMTK_362_PRIVATE_DIC CACHE)
  unset(USE_SYSTEM_DCMTK CACHE)
  unset(ENABLE_DCMTK_JPEG CACHE)
  unset(ENABLE_DCMTK_JPEG_LOSSLESS CACHE)
  unset(DCMTK_STATIC_VERSION CACHE)
  unset(ENABLE_DCMTK_LOG CACHE)
endif()


#####################################################################
## List of source files
#####################################################################

set(ORTHANC_CORE_SOURCES_INTERNAL
  ${ORTHANC_ROOT}/OrthancFramework/Sources/Cache/MemoryCache.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/Cache/MemoryObjectCache.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/Cache/MemoryStringCache.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/ChunkedBuffer.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomFormat/DicomTag.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/EnumerationDictionary.h
  ${ORTHANC_ROOT}/OrthancFramework/Sources/Enumerations.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/FileStorage/MemoryStorageArea.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/MultipartStreamReader.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/StringMatcher.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/Logging.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/OrthancFramework.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/SerializationToolbox.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/Toolbox.cpp
  ${ORTHANC_ROOT}/OrthancFramework/Sources/WebServiceParameters.cpp
  )

if (ENABLE_MODULE_IMAGES)
  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/Font.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/FontRegistry.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/IImageWriter.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/Image.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/ImageAccessor.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/ImageBuffer.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/ImageProcessing.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/PamReader.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/PamWriter.cpp
    )
endif()

if (ENABLE_MODULE_DICOM)
  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomFormat/DicomArray.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomFormat/DicomImageInformation.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomFormat/DicomInstanceHasher.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomFormat/DicomIntegerPixelAccessor.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomFormat/DicomMap.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomFormat/DicomValue.cpp
    )
endif()

if (ENABLE_MODULE_JOBS)
  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/GenericJobUnserializer.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/JobInfo.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/JobStatus.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/JobStepResult.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/Operations/JobOperationValues.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/Operations/LogJobOperation.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/Operations/SequenceOfOperationsJob.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/SetOfCommandsJob.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/SetOfInstancesJob.cpp
    )
endif()



#####################################################################
## Configuration of optional third-party dependencies
#####################################################################


##
## Embedded database: SQLite
##

if (ENABLE_SQLITE)
  include(${CMAKE_CURRENT_LIST_DIR}/SQLiteConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_SQLITE=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/SQLite/Connection.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/SQLite/FunctionContext.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/SQLite/Statement.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/SQLite/StatementId.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/SQLite/StatementReference.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/SQLite/Transaction.cpp
    )
endif()


##
## Cryptography: OpenSSL and libp11
## Must be above "ENABLE_WEB_CLIENT" and "ENABLE_WEB_SERVER"
##

if (ENABLE_CRYPTO_OPTIONS)
  if (ENABLE_SSL)
    include(${CMAKE_CURRENT_LIST_DIR}/OpenSslConfiguration.cmake)
    add_definitions(-DORTHANC_ENABLE_SSL=1)
  else()
    unset(ENABLE_OPENSSL_ENGINES CACHE)
    unset(USE_SYSTEM_OPENSSL CACHE)
    add_definitions(-DORTHANC_ENABLE_SSL=0)
  endif()

  if (ENABLE_PKCS11)
    if (ENABLE_SSL)
      include(${CMAKE_CURRENT_LIST_DIR}/LibP11Configuration.cmake)

      add_definitions(-DORTHANC_ENABLE_PKCS11=1)
      list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
        ${ORTHANC_ROOT}/OrthancFramework/Sources/Pkcs11.cpp
        )
    else()
      message(FATAL_ERROR "OpenSSL is required to enable PKCS#11 support")
    endif()
  else()
    add_definitions(-DORTHANC_ENABLE_PKCS11=0)  
  endif()
endif()


##
## HTTP client: libcurl
##

if (ENABLE_WEB_CLIENT)
  include(${CMAKE_CURRENT_LIST_DIR}/LibCurlConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_CURL=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpClient.cpp
    )
endif()


##
## HTTP server: Mongoose 3.8 or Civetweb
##

if (ENABLE_WEB_SERVER)
  if (ENABLE_CIVETWEB)
    include(${CMAKE_CURRENT_LIST_DIR}/CivetwebConfiguration.cmake)
    add_definitions(
      -DORTHANC_ENABLE_CIVETWEB=1
      -DORTHANC_ENABLE_MONGOOSE=0
      )
    set(ORTHANC_ENABLE_CIVETWEB 1)
  else()
    include(${CMAKE_CURRENT_LIST_DIR}/MongooseConfiguration.cmake)
  endif()

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/BufferHttpSender.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/FilesystemHttpHandler.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/FilesystemHttpSender.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/HttpContentNegociation.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/HttpFileSender.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/HttpOutput.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/HttpServer.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/HttpStreamTranscoder.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/HttpToolbox.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/HttpServer/StringHttpOutput.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/RestApi/RestApi.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/RestApi/RestApiCall.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/RestApi/RestApiGetCall.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/RestApi/RestApiHierarchy.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/RestApi/RestApiOutput.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/RestApi/RestApiPath.cpp
    )
endif()

if (ORTHANC_ENABLE_CIVETWEB)
  add_definitions(-DORTHANC_ENABLE_CIVETWEB=1)
else()
  add_definitions(-DORTHANC_ENABLE_CIVETWEB=0)
endif()

if (ORTHANC_ENABLE_MONGOOSE)
  add_definitions(-DORTHANC_ENABLE_MONGOOSE=1)
else()
  add_definitions(-DORTHANC_ENABLE_MONGOOSE=0)
endif()



##
## JPEG support: libjpeg
##

if (ENABLE_JPEG)
  if (NOT ENABLE_MODULE_IMAGES)
    message(FATAL_ERROR "Image processing primitives must be enabled if enabling libjpeg support")
  endif()

  include(${CMAKE_CURRENT_LIST_DIR}/LibJpegConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_JPEG=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/JpegErrorManager.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/JpegReader.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/JpegWriter.cpp
    )
endif()


##
## zlib support
##

if (ENABLE_ZLIB)
  include(${CMAKE_CURRENT_LIST_DIR}/ZlibConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_ZLIB=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Compression/DeflateBaseCompressor.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Compression/GzipCompressor.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Compression/ZlibCompressor.cpp
    )

  if (NOT ORTHANC_SANDBOXED)
    list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
      ${ORTHANC_ROOT}/OrthancFramework/Sources/Compression/HierarchicalZipWriter.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/Compression/ZipWriter.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/FileStorage/StorageAccessor.cpp
      )
  endif()
endif()


##
## PNG support: libpng (in conjunction with zlib)
##

if (ENABLE_PNG)
  if (NOT ENABLE_ZLIB)
    message(FATAL_ERROR "Support for zlib must be enabled if enabling libpng support")
  endif()

  if (NOT ENABLE_MODULE_IMAGES)
    message(FATAL_ERROR "Image processing primitives must be enabled if enabling libpng support")
  endif()
  
  include(${CMAKE_CURRENT_LIST_DIR}/LibPngConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_PNG=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/PngReader.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Images/PngWriter.cpp
    )
endif()


##
## Lua support
##

if (ENABLE_LUA)
  include(${CMAKE_CURRENT_LIST_DIR}/LuaConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_LUA=1)

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Lua/LuaContext.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Lua/LuaFunctionCall.cpp
    )
endif()


##
## XML support: pugixml
##

if (ENABLE_PUGIXML)
  include(${CMAKE_CURRENT_LIST_DIR}/PugixmlConfiguration.cmake)
  add_definitions(-DORTHANC_ENABLE_PUGIXML=1)
endif()


##
## Locale support
##

if (ENABLE_LOCALE)
  if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    # In WebAssembly or asm.js, we rely on the version of iconv that
    # is shipped with the stdlib
    unset(BOOST_LOCALE_BACKEND CACHE)
  else()
    if (BOOST_LOCALE_BACKEND STREQUAL "gcc")
    elseif (BOOST_LOCALE_BACKEND STREQUAL "libiconv")
      include(${CMAKE_CURRENT_LIST_DIR}/LibIconvConfiguration.cmake)
    elseif (BOOST_LOCALE_BACKEND STREQUAL "icu")
      include(${CMAKE_CURRENT_LIST_DIR}/LibIcuConfiguration.cmake)
    elseif (BOOST_LOCALE_BACKEND STREQUAL "wconv")
      message("Using Microsoft Window's wconv")
    else()
      message(FATAL_ERROR "Invalid value for BOOST_LOCALE_BACKEND: ${BOOST_LOCALE_BACKEND}")
    endif()
  endif()
  
  add_definitions(-DORTHANC_ENABLE_LOCALE=1)
endif()


##
## Google Test for unit testing
##

if (ENABLE_GOOGLE_TEST)
  include(${CMAKE_CURRENT_LIST_DIR}/GoogleTestConfiguration.cmake)
endif()



#####################################################################
## Inclusion of mandatory third-party dependencies
#####################################################################

include(${CMAKE_CURRENT_LIST_DIR}/JsonCppConfiguration.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/UuidConfiguration.cmake)

# We put Boost as the last dependency, as it is the heaviest to
# configure, which allows one to quickly spot problems when configuring
# static builds in other dependencies
include(${CMAKE_CURRENT_LIST_DIR}/BoostConfiguration.cmake)


#####################################################################
## Optional configuration of DCMTK
#####################################################################

if (ENABLE_DCMTK)
  if (NOT ENABLE_LOCALE)
    message(FATAL_ERROR "Support for locales must be enabled if enabling DCMTK support")
  endif()

  if (NOT ENABLE_MODULE_DICOM)
    message(FATAL_ERROR "DICOM module must be enabled if enabling DCMTK support")
  endif()

  include(${CMAKE_CURRENT_LIST_DIR}/DcmtkConfiguration.cmake)

  add_definitions(-DORTHANC_ENABLE_DCMTK=1)

  if (ENABLE_DCMTK_JPEG)
    add_definitions(-DORTHANC_ENABLE_DCMTK_JPEG=1)
  else()
    add_definitions(-DORTHANC_ENABLE_DCMTK_JPEG=0)
  endif()

  if (ENABLE_DCMTK_JPEG_LOSSLESS)
    add_definitions(-DORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS=1)
  else()
    add_definitions(-DORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS=0)
  endif()

  set(ORTHANC_DICOM_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/DicomModification.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/DicomWebJsonVisitor.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/ParsedDicomDir.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/ParsedDicomFile.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/ToDcmtkBridge.cpp

    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/Internals/DicomFrameIndex.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/Internals/DicomImageDecoder.cpp
    )

  if (NOT ORTHANC_SANDBOXED)
    list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/DicomDirWriter.cpp
      )
  endif()

  if (ENABLE_DCMTK_NETWORKING)
    add_definitions(-DORTHANC_ENABLE_DCMTK_NETWORKING=1)
    list(APPEND ORTHANC_DICOM_SOURCES_INTERNAL
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/DicomAssociation.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/DicomAssociationParameters.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/DicomControlUserConnection.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/DicomFindAnswers.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/DicomServer.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/DicomStoreUserConnection.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/Internals/CommandDispatcher.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/Internals/FindScp.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/Internals/MoveScp.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/Internals/GetScp.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/Internals/StoreScp.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/RemoteModalityParameters.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomNetworking/TimeoutDicomConnectionManager.cpp
      )
  else()
    add_definitions(-DORTHANC_ENABLE_DCMTK_NETWORKING=0)
  endif()

  # New in Orthanc 1.6.0
  if (ENABLE_DCMTK_TRANSCODING)
    add_definitions(-DORTHANC_ENABLE_DCMTK_TRANSCODING=1)
    list(APPEND ORTHANC_DICOM_SOURCES_INTERNAL
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/DcmtkTranscoder.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/IDicomTranscoder.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/DicomParsing/MemoryBufferTranscoder.cpp
      )
  else()
    add_definitions(-DORTHANC_ENABLE_DCMTK_TRANSCODING=0)
  endif()
endif()


#####################################################################
## Configuration of the C/C++ macros
#####################################################################

add_definitions(
  -DORTHANC_API_VERSION=${ORTHANC_API_VERSION}
  -DORTHANC_DATABASE_VERSION=${ORTHANC_DATABASE_VERSION}
  -DORTHANC_DEFAULT_DICOM_ENCODING=Encoding_Latin1
  -DORTHANC_ENABLE_BASE64=1
  -DORTHANC_ENABLE_MD5=1
  -DORTHANC_MAXIMUM_TAG_LENGTH=256
  -DORTHANC_VERSION="${ORTHANC_VERSION}"
  )


if (ORTHANC_BUILDING_FRAMEWORK_LIBRARY)
  add_definitions(-DORTHANC_BUILDING_FRAMEWORK_LIBRARY=1)
else()
  add_definitions(-DORTHANC_BUILDING_FRAMEWORK_LIBRARY=0)
endif()


if (ORTHANC_SANDBOXED)
  add_definitions(
    -DORTHANC_SANDBOXED=1
    )

  if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(ORTHANC_ENABLE_LOGGING ON)
    set(ORTHANC_ENABLE_LOGGING_STDIO ON)
  else()
    set(ORTHANC_ENABLE_LOGGING OFF)
  endif()
  
else()
  set(ORTHANC_ENABLE_LOGGING ON)
  set(ORTHANC_ENABLE_LOGGING_STDIO OFF)

  add_definitions(
    -DORTHANC_SANDBOXED=0
    )

  list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
    ${ORTHANC_ROOT}/OrthancFramework/Sources/Cache/SharedArchive.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/FileBuffer.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/FileStorage/FilesystemStorage.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/MetricsRegistry.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/MultiThreading/RunnableWorkersPool.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/MultiThreading/Semaphore.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/MultiThreading/SharedMessageQueue.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/SharedLibrary.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/SystemToolbox.cpp
    ${ORTHANC_ROOT}/OrthancFramework/Sources/TemporaryFile.cpp
    )

  if (ENABLE_MODULE_JOBS)
    list(APPEND ORTHANC_CORE_SOURCES_INTERNAL
      ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/JobsEngine.cpp
      ${ORTHANC_ROOT}/OrthancFramework/Sources/JobsEngine/JobsRegistry.cpp
      )
  endif()
endif()



if (ORTHANC_ENABLE_LOGGING)
  add_definitions(-DORTHANC_ENABLE_LOGGING=1)
else()
  add_definitions(-DORTHANC_ENABLE_LOGGING=0)
endif()

if (ORTHANC_ENABLE_LOGGING_STDIO)
  add_definitions(-DORTHANC_ENABLE_LOGGING_STDIO=1)
else()
  add_definitions(-DORTHANC_ENABLE_LOGGING_STDIO=0)
endif()



#####################################################################
## Configuration of Orthanc versioning macros (new in Orthanc 1.5.0)
#####################################################################

if (ORTHANC_VERSION STREQUAL "mainline")
  set(ORTHANC_VERSION_MAJOR "999")
  set(ORTHANC_VERSION_MINOR "999")
  set(ORTHANC_VERSION_REVISION "999")
else()
  string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)$" "\\1" ORTHANC_VERSION_MAJOR    ${ORTHANC_VERSION})
  string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)$" "\\2" ORTHANC_VERSION_MINOR    ${ORTHANC_VERSION})
  string(REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)$" "\\3" ORTHANC_VERSION_REVISION ${ORTHANC_VERSION})

  if (NOT ORTHANC_VERSION STREQUAL
      "${ORTHANC_VERSION_MAJOR}.${ORTHANC_VERSION_MINOR}.${ORTHANC_VERSION_REVISION}")
    message(FATAL_ERROR "Error in the (x.y.z) format of the Orthanc version: ${ORTHANC_VERSION}")
  endif()
endif()

add_definitions(
  -DORTHANC_VERSION_MAJOR=${ORTHANC_VERSION_MAJOR}
  -DORTHANC_VERSION_MINOR=${ORTHANC_VERSION_MINOR}
  -DORTHANC_VERSION_REVISION=${ORTHANC_VERSION_REVISION}
  )



#####################################################################
## Gathering of all the source code
#####################################################################

# The "xxx_INTERNAL" variables list the source code that belongs to
# the Orthanc project. It can be used to configure precompiled headers
# if using Microsoft Visual Studio.

# The "xxx_DEPENDENCIES" variables list the source code coming from
# third-party dependencies.


set(ORTHANC_CORE_SOURCES_DEPENDENCIES
  ${BOOST_SOURCES}
  ${CIVETWEB_SOURCES}
  ${CURL_SOURCES}
  ${JSONCPP_SOURCES}
  ${LIBICONV_SOURCES}
  ${LIBICU_SOURCES}
  ${LIBJPEG_SOURCES}
  ${LIBP11_SOURCES}
  ${LIBPNG_SOURCES}
  ${LUA_SOURCES}
  ${MONGOOSE_SOURCES}
  ${OPENSSL_SOURCES}
  ${PUGIXML_SOURCES}
  ${SQLITE_SOURCES}
  ${UUID_SOURCES}
  ${ZLIB_SOURCES}

  ${ORTHANC_ROOT}/OrthancFramework/Resources/ThirdParty/md5/md5.c
  ${ORTHANC_ROOT}/OrthancFramework/Resources/ThirdParty/base64/base64.cpp
  )

if (ENABLE_ZLIB AND NOT ORTHANC_SANDBOXED)
  list(APPEND ORTHANC_CORE_SOURCES_DEPENDENCIES
    # This is the minizip distribution to create ZIP files using zlib
    ${ORTHANC_ROOT}/OrthancFramework/Resources/ThirdParty/minizip/ioapi.c
    ${ORTHANC_ROOT}/OrthancFramework/Resources/ThirdParty/minizip/zip.c
    )
endif()


if (NOT "${LIBICU_RESOURCES}" STREQUAL "" OR
    NOT "${DCMTK_DICTIONARIES}" STREQUAL "")
  EmbedResources(
    --namespace=Orthanc.FrameworkResources
    --target=OrthancFrameworkResources
    --framework-path=${ORTHANC_ROOT}/OrthancFramework/Sources
    ${LIBICU_RESOURCES}
    ${DCMTK_DICTIONARIES}
    )
endif()


set(ORTHANC_CORE_SOURCES
  ${ORTHANC_CORE_SOURCES_INTERNAL}
  ${ORTHANC_CORE_SOURCES_DEPENDENCIES}
  )

if (ENABLE_DCMTK)
  list(APPEND ORTHANC_DICOM_SOURCES_DEPENDENCIES
    ${DCMTK_SOURCES}
    )
  
  set(ORTHANC_DICOM_SOURCES
    ${ORTHANC_DICOM_SOURCES_INTERNAL}
    ${ORTHANC_DICOM_SOURCES_DEPENDENCIES}
    )
endif()