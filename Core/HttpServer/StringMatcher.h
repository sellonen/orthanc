/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2019 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

namespace Orthanc
{
  // Convenience class that wraps a Boost algorithm for string matching
  class StringMatcher : public boost::noncopyable
  {
  public:
    typedef std::string::const_iterator Iterator;

  private:
    class Search;
      
    // WARNING - The lifetime of "pattern_" must be larger than
    // "search_", as the latter references "pattern_"
    boost::shared_ptr<Search>  search_;  // PImpl pattern
    std::string                pattern_;
    bool                       valid_;
    Iterator                   matchBegin_;
    Iterator                   matchEnd_;
    
  public:
    StringMatcher(const std::string& pattern);

    const std::string& GetPattern() const
    {
      return pattern_;
    }

    bool IsValid() const
    {
      return valid_;
    }

    bool Apply(Iterator start,
               Iterator end);

    bool Apply(const std::string& corpus)
    {
      return Apply(corpus.begin(), corpus.end());
    }

    Iterator GetMatchBegin() const;

    Iterator GetMatchEnd() const;

    const char* GetPointerBegin() const;

    const char* GetPointerEnd() const;
  };
}
