/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "PrecompiledHeaders.h"
#include "SystemToolbox.h"


#if defined(_WIN32)
#  include <windows.h>
#  include <process.h>   // For "_spawnvp()" and "_getpid()"
#  include <stdlib.h>    // For "environ"
#else
#  include <unistd.h>    // For "execvp()"
#  include <sys/wait.h>  // For "waitpid()"
#endif


#if defined(__APPLE__) && defined(__MACH__)
#  include <mach-o/dyld.h> /* _NSGetExecutablePath */
#  include <limits.h>      /* PATH_MAX */
#endif


#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
#  include <limits.h>      /* PATH_MAX */
#  include <signal.h>
#  include <unistd.h>
#endif


#if defined(__OpenBSD__)
#  include <sys/sysctl.h>  // For "sysctl", "CTL_KERN" and "KERN_PROC_ARGS"
#endif


#include "Logging.h"
#include "OrthancException.h"
#include "Toolbox.h"

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>


/*=========================================================================
  The section below comes from the Boost 1.68.0 project:
  https://github.com/boostorg/program_options/blob/boost-1.68.0/src/parsers.cpp
  
  Copyright Vladimir Prus 2002-2004.
  Distributed under the Boost Software License, Version 1.0.
  (See accompanying file LICENSE_1_0.txt
  or copy at http://www.boost.org/LICENSE_1_0.txt)
  =========================================================================*/

// The 'environ' should be declared in some cases. E.g. Linux man page says:
// (This variable must be declared in the user program, but is declared in 
// the header file unistd.h in case the header files came from libc4 or libc5, 
// and in case they came from glibc and _GNU_SOURCE was defined.) 
// To be safe, declare it here.

// It appears that on Mac OS X the 'environ' variable is not
// available to dynamically linked libraries.
// See: http://article.gmane.org/gmane.comp.lib.boost.devel/103843
// See: http://lists.gnu.org/archive/html/bug-guile/2004-01/msg00013.html
#if defined(__APPLE__) && defined(__DYNAMIC__)
// The proper include for this is crt_externs.h, however it's not
// available on iOS. The right replacement is not known. See
// https://svn.boost.org/trac/boost/ticket/5053
extern "C"
{
  extern char ***_NSGetEnviron(void);
}
#  define environ (*_NSGetEnviron()) 
#else
#  if defined(__MWERKS__)
#    include <crtl.h>
#  else
#    if !defined(_WIN32) || defined(__COMO_VERSION__)
extern char** environ;
#    endif
#  endif
#endif


/*=========================================================================
  End of section from the Boost 1.68.0 project
  =========================================================================*/


namespace Orthanc
{
  static bool finish_;
  static ServerBarrierEvent barrierEvent_;

#if defined(_WIN32)
  static BOOL WINAPI ConsoleControlHandler(DWORD dwCtrlType)
  {
    // http://msdn.microsoft.com/en-us/library/ms683242(v=vs.85).aspx
    finish_ = true;
    return true;
  }
#else
  static void SignalHandler(int signal)
  {
    if (signal == SIGHUP)
    {
      barrierEvent_ = ServerBarrierEvent_Reload;
    }

    finish_ = true;
  }
#endif


  static ServerBarrierEvent ServerBarrierInternal(const bool* stopFlag)
  {
#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleControlHandler, true);
#else
    signal(SIGINT, SignalHandler);
    signal(SIGQUIT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGHUP, SignalHandler);
#endif
  
    // Active loop that awakens every 100ms
    finish_ = false;
    barrierEvent_ = ServerBarrierEvent_Stop;
    while (!(*stopFlag || finish_))
    {
      SystemToolbox::USleep(100 * 1000);
    }

#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleControlHandler, false);
#else
    signal(SIGINT, NULL);
    signal(SIGQUIT, NULL);
    signal(SIGTERM, NULL);
    signal(SIGHUP, NULL);
#endif

    return barrierEvent_;
  }


  ServerBarrierEvent SystemToolbox::ServerBarrier(const bool& stopFlag)
  {
    return ServerBarrierInternal(&stopFlag);
  }


  ServerBarrierEvent SystemToolbox::ServerBarrier()
  {
    const bool stopFlag = false;
    return ServerBarrierInternal(&stopFlag);
  }


  void SystemToolbox::USleep(uint64_t microSeconds)
  {
#if defined(_WIN32)
    ::Sleep(static_cast<DWORD>(microSeconds / static_cast<uint64_t>(1000)));
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__native_client__)
    usleep(microSeconds);
#else
#error Support your platform here
#endif
  }


  static std::streamsize GetStreamSize(std::istream& f)
  {
    // http://www.cplusplus.com/reference/iostream/istream/tellg/
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    return size;
  }


  void SystemToolbox::ReadFile(std::string& content,
                               const std::string& path,
                               bool log)
  {
    if (!IsRegularFile(path))
    {
      throw OrthancException(ErrorCode_RegularFileExpected,
                             "The path does not point to a regular file: " + path,
                             log);
    }

    boost::filesystem::ifstream f;
    f.open(path, std::ifstream::in | std::ifstream::binary);
    if (!f.good())
    {
      throw OrthancException(ErrorCode_InexistentFile,
                             "File not found: " + path,
                             log);
    }

    std::streamsize size = GetStreamSize(f);
    content.resize(static_cast<size_t>(size));
    if (size != 0)
    {
      f.read(&content[0], size);
    }

    f.close();
  }


  bool SystemToolbox::ReadHeader(std::string& header,
                                 const std::string& path,
                                 size_t headerSize)
  {
    if (!IsRegularFile(path))
    {
      throw OrthancException(ErrorCode_RegularFileExpected,
                             "The path does not point to a regular file: " + path);
    }

    boost::filesystem::ifstream f;
    f.open(path, std::ifstream::in | std::ifstream::binary);
    if (!f.good())
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }

    bool full = true;

    {
      std::streamsize size = GetStreamSize(f);
      if (size <= 0)
      {
        headerSize = 0;
        full = false;
      }
      else if (static_cast<size_t>(size) < headerSize)
      {
        headerSize = static_cast<size_t>(size);  // Truncate to the size of the file
        full = false;
      }
    }

    header.resize(headerSize);
    if (headerSize != 0)
    {
      f.read(&header[0], headerSize);
    }

    f.close();

    return full;
  }


  void SystemToolbox::WriteFile(const void* content,
                                size_t size,
                                const std::string& path,
                                bool callFsync)
  {
    //boost::filesystem::ofstream f;
    boost::iostreams::stream<boost::iostreams::file_descriptor_sink> f;
    
    f.open(path, std::ofstream::out | std::ofstream::binary);
    if (!f.good())
    {
      throw OrthancException(ErrorCode_CannotWriteFile);
    }

    if (size != 0)
    {
      f.write(reinterpret_cast<const char*>(content), size);

      if (!f.good())
      {
        f.close();
        throw OrthancException(ErrorCode_FileStorageCannotWrite);
      }
    }

    if (callFsync)
    {
      // https://stackoverflow.com/a/23826489/881731
      f.flush();

      bool success = false;

      /**
       * "f->handle()" corresponds to "FILE*" (aka "HANDLE") on
       * Microsoft Windows, and to "int" (file descriptor) on other
       * systems:
       * https://github.com/boostorg/iostreams/blob/develop/include/boost/iostreams/detail/file_handle.hpp
       **/
      
#if defined(_WIN32)
      // https://docs.microsoft.com/fr-fr/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers
      success = (::FlushFileBuffers(f->handle()) != 0);
#elif (_POSIX_C_SOURCE >= 199309L || _XOPEN_SOURCE >= 500)
      success = (::fdatasync(f->handle()) == 0);
#else
      success = (::fsync(f->handle()) == 0);
#endif

      if (!success)
      {
        throw OrthancException(ErrorCode_FileStorageCannotWrite, "Cannot force flush to disk");
      }
    }

    f.close();
  }


  void SystemToolbox::WriteFile(const std::string& content,
                                const std::string& path,
                                bool callFsync)
  {
    WriteFile(content.size() > 0 ? content.c_str() : NULL,
              content.size(), path, callFsync);
  }


  void SystemToolbox::RemoveFile(const std::string& path)
  {
    if (boost::filesystem::exists(path))
    {
      if (IsRegularFile(path))
      {
        boost::filesystem::remove(path);
      }
      else
      {
        throw OrthancException(ErrorCode_RegularFileExpected);
      }
    }
  }


  uint64_t SystemToolbox::GetFileSize(const std::string& path)
  {
    try
    {
      return static_cast<uint64_t>(boost::filesystem::file_size(path));
    }
    catch (boost::filesystem::filesystem_error&)
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }
  }


  void SystemToolbox::MakeDirectory(const std::string& path)
  {
    if (boost::filesystem::exists(path))
    {
      if (!boost::filesystem::is_directory(path))
      {
        throw OrthancException(ErrorCode_DirectoryOverFile);
      }
    }
    else
    {
      if (!boost::filesystem::create_directories(path))
      {
        throw OrthancException(ErrorCode_MakeDirectory);
      }
    }
  }


  bool SystemToolbox::IsExistingFile(const std::string& path)
  {
    return boost::filesystem::exists(path);
  }


#if defined(_WIN32)
  static std::string GetPathToExecutableInternal()
  {
    // Yes, this is ugly, but there is no simple way to get the 
    // required buffer size, so we use a big constant
    std::vector<char> buffer(32768);
    /*int bytes =*/ GetModuleFileNameA(NULL, &buffer[0], static_cast<DWORD>(buffer.size() - 1));
    return std::string(&buffer[0]);
  }

#elif defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
  static std::string GetPathToExecutableInternal()
  {
    // NOTE: For FreeBSD, using KERN_PROC_PATHNAME might be a better alternative

    std::vector<char> buffer(PATH_MAX + 1);
    ssize_t bytes = readlink("/proc/self/exe", &buffer[0], buffer.size() - 1);
    if (bytes == 0)
    {
      throw OrthancException(ErrorCode_PathToExecutable);
    }

    return std::string(&buffer[0]);
  }

#elif defined(__APPLE__) && defined(__MACH__)
  static std::string GetPathToExecutableInternal()
  {
    char pathbuf[PATH_MAX + 1];
    unsigned int  bufsize = static_cast<int>(sizeof(pathbuf));

    _NSGetExecutablePath( pathbuf, &bufsize);

    return std::string(pathbuf);
  }

#elif defined(__OpenBSD__)
  static std::string GetPathToExecutableInternal()
  {
    // This is an adapted version of the patch proposed in issue #64
    // without an explicit call to "malloc()" to prevent memory leak
    // https://bitbucket.org/sjodogne/orthanc/issues/64/add-openbsd-support
    // https://stackoverflow.com/q/31494901/881731

    const int mib[4] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };

    size_t len;
    if (sysctl(mib, 4, NULL, &len, NULL, 0) == -1) 
    {
      throw OrthancException(ErrorCode_PathToExecutable);
    }

    std::string tmp;
    tmp.resize(len);

    char** buffer = reinterpret_cast<char**>(&tmp[0]);

    if (sysctl(mib, 4, buffer, &len, NULL, 0) == -1) 
    {
      throw OrthancException(ErrorCode_PathToExecutable);
    }
    else
    {
      return std::string(buffer[0]);
    }
  }

#else
#error Support your platform here
#endif


  std::string SystemToolbox::GetPathToExecutable()
  {
    boost::filesystem::path p(GetPathToExecutableInternal());
    return boost::filesystem::absolute(p).string();
  }


  std::string SystemToolbox::GetDirectoryOfExecutable()
  {
    boost::filesystem::path p(GetPathToExecutableInternal());
    return boost::filesystem::absolute(p.parent_path()).string();
  }


  void SystemToolbox::ExecuteSystemCommand(const std::string& command,
                                           const std::vector<std::string>& arguments)
  {
    // Convert the arguments as a C array
    std::vector<char*>  args(arguments.size() + 2);

    args.front() = const_cast<char*>(command.c_str());

    for (size_t i = 0; i < arguments.size(); i++)
    {
      args[i + 1] = const_cast<char*>(arguments[i].c_str());
    }

    args.back() = NULL;

    int status;

#if defined(_WIN32)
    // http://msdn.microsoft.com/en-us/library/275khfab.aspx
    status = static_cast<int>(_spawnvp(_P_OVERLAY, command.c_str(), &args[0]));

#else
    int pid = fork();

    if (pid == -1)
    {
      // Error in fork()
      throw OrthancException(ErrorCode_SystemCommand, "Cannot fork a child process");
    }
    else if (pid == 0)
    {
      // Execute the system command in the child process
      execvp(command.c_str(), &args[0]);

      // We should never get here
      _exit(1);
    }
    else
    {
      // Wait for the system command to exit
      waitpid(pid, &status, 0);
    }
#endif

    if (status != 0)
    {
      throw OrthancException(ErrorCode_SystemCommand,
                             "System command failed with status code " +
                             boost::lexical_cast<std::string>(status));
    }
  }


  int SystemToolbox::GetProcessId()
  {
#if defined(_WIN32)
    return static_cast<int>(_getpid());
#else
    return static_cast<int>(getpid());
#endif
  }


  bool SystemToolbox::IsRegularFile(const std::string& path)
  {
    namespace fs = boost::filesystem;

    try
    {
      if (fs::exists(path))
      {
        fs::file_status status = fs::status(path);
        return (status.type() == boost::filesystem::regular_file ||
                status.type() == boost::filesystem::reparse_file);   // Fix BitBucket issue #11
      }
    }
    catch (fs::filesystem_error&)
    {
    }

    return false;
  }


  FILE* SystemToolbox::OpenFile(const std::string& path,
                                FileMode mode)
  {
#if defined(_WIN32)
    // TODO Deal with special characters by converting to the current locale
#endif

    const char* m;
    switch (mode)
    {
      case FileMode_ReadBinary:
        m = "rb";
        break;

      case FileMode_WriteBinary:
        m = "wb";
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    return fopen(path.c_str(), m);
  }


  static boost::posix_time::ptime GetNow(bool utc)
  {
    if (utc)
    {
      return boost::posix_time::second_clock::universal_time();
    }
    else
    {
      return boost::posix_time::second_clock::local_time();
    }
  }


  std::string SystemToolbox::GetNowIsoString(bool utc)
  {
    return boost::posix_time::to_iso_string(GetNow(utc));
  }

  
  void SystemToolbox::GetNowDicom(std::string& date,
                                  std::string& time,
                                  bool utc)
  {
    boost::posix_time::ptime now = GetNow(utc);
    tm tm = boost::posix_time::to_tm(now);

    char s[32];
    sprintf(s, "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    date.assign(s);

    // TODO milliseconds
    sprintf(s, "%02d%02d%02d.%06d", tm.tm_hour, tm.tm_min, tm.tm_sec, 0);
    time.assign(s);
  }

  
  unsigned int SystemToolbox::GetHardwareConcurrency()
  {
    // Get the number of available hardware threads (e.g. number of
    // CPUs or cores or hyperthreading units)
    unsigned int threads = boost::thread::hardware_concurrency();
    
    if (threads <= 0)
    {
      return 1;
    }
    else
    {
      return threads;
    }
  }


  MimeType SystemToolbox::AutodetectMimeType(const std::string& path)
  {
    std::string extension = boost::filesystem::extension(path);
    Toolbox::ToLowerCase(extension);

    // http://en.wikipedia.org/wiki/Mime_types
    // Text types
    if (extension == ".txt")
    {
      return MimeType_PlainText;
    }
    else if (extension == ".html")
    {
      return MimeType_Html;
    }
    else if (extension == ".xml")
    {
      return MimeType_Xml;
    }
    else if (extension == ".css")
    {
      return MimeType_Css;
    }

    // Application types
    else if (extension == ".js")
    {
      return MimeType_JavaScript;
    }
    else if (extension == ".json" ||
             extension == ".nmf"  /* manifest */)
    {
      return MimeType_Json;
    }
    else if (extension == ".pdf")
    {
      return MimeType_Pdf;
    }
    else if (extension == ".wasm")
    {
      return MimeType_WebAssembly;
    }
    else if (extension == ".nexe")
    {
      return MimeType_NaCl;
    }
    else if (extension == ".pexe")
    {
      return MimeType_PNaCl;
    }

    // Images types
    else if (extension == ".jpg" ||
             extension == ".jpeg")
    {
      return MimeType_Jpeg;
    }
    else if (extension == ".gif")
    {
      return MimeType_Gif;
    }
    else if (extension == ".png")
    {
      return MimeType_Png;
    }
    else if (extension == ".pam")
    {
      return MimeType_Pam;
    }
    else if (extension == ".svg")
    {
      return MimeType_Svg;
    }

    // Various types
    else if (extension == ".woff")
    {
      return MimeType_Woff;
    }
    else if (extension == ".woff2")
    {
      return MimeType_Woff2;
    }

    // Default type
    else
    {
      LOG(INFO) << "Unknown MIME type for extension \"" << extension << "\"";
      return MimeType_Binary;
    }
  }


  void SystemToolbox::GetEnvironmentVariables(std::map<std::string, std::string>& env)
  {
    env.clear();
    
    for (char **p = environ; *p != NULL; p++)
    {
      std::string v(*p);
      size_t pos = v.find('=');

      if (pos != std::string::npos)
      {
        std::string key = v.substr(0, pos);
        std::string value = v.substr(pos + 1);
        env[key] = value;
      } 
    }
  }


  std::string SystemToolbox::InterpretRelativePath(const std::string& baseDirectory,
                                                   const std::string& relativePath)
  {
    boost::filesystem::path base(baseDirectory);
    boost::filesystem::path relative(relativePath);

    /**
       The following lines should be equivalent to this one: 

       return (base / relative).string();

       However, for some unknown reason, some versions of Boost do not
       make the proper path resolution when "baseDirectory" is an
       absolute path. So, a hack is used below.
    **/

    if (relative.is_absolute())
    {
      return relative.string();
    }
    else
    {
      return (base / relative).string();
    }
  }
}
