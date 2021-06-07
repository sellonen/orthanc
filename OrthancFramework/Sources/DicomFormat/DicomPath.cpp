/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#include "../PrecompiledHeaders.h"
#include "DicomPath.h"


#if !defined(ORTHANC_ENABLE_DCMTK)
#  error Macro ORTHANC_ENABLE_DCMTK must be defined
#endif

#if ORTHANC_ENABLE_DCMTK == 1
#  include "../DicomParsing/FromDcmtkBridge.h"
#endif

#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  DicomPath::PrefixItem::PrefixItem(DicomTag tag,
                                    bool isUniversal,
                                    size_t index) :
    tag_(tag),
    isUniversal_(isUniversal),
    index_(index)
  {
  }
      

  size_t DicomPath::PrefixItem::GetIndex() const
  {
    if (isUniversal_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return index_;
    }
  }

  
  DicomTag DicomPath::ParseTag(const std::string& token)
  {
    DicomTag tag(0,0);
            
    if (token[0] == '(' &&
        token[token.size() - 1] == ')')
    {
      std::string hex = token.substr(1, token.size() - 2);
      if (!DicomTag::ParseHexadecimal(tag, hex.c_str()))
      {
        throw OrthancException(ErrorCode_UnknownDicomTag, "Cannot parse tag: " + token);
      }
    }
    else
    {
#if ORTHANC_ENABLE_DCMTK == 1
      tag = FromDcmtkBridge::ParseTag(token);
#else
      if (!DicomTag::ParseHexadecimal(tag, token.c_str()))
      {
        throw OrthancException(ErrorCode_UnknownDicomTag, "Cannot parse tag without DCMTK: " + token);
      }
#endif
    }

    return tag;
  }


  const DicomPath::PrefixItem& DicomPath::GetLevel(size_t i) const
  {
    if (i >= prefix_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return prefix_[i];
    }
  }


  DicomPath::DicomPath(const Orthanc::DicomTag& sequence,
                       size_t index,
                       const Orthanc::DicomTag& tag) :
    finalTag_(tag)
  {
    AddIndexedTagToPrefix(sequence, index);
  }

  
  DicomPath::DicomPath(const Orthanc::DicomTag& sequence1,
                       size_t index1,
                       const Orthanc::DicomTag& sequence2,
                       size_t index2,
                       const Orthanc::DicomTag& tag) :
    finalTag_(tag)
  {
    AddIndexedTagToPrefix(sequence1, index1);
    AddIndexedTagToPrefix(sequence2, index2);
  }


  DicomPath::DicomPath(const Orthanc::DicomTag& sequence1,
                       size_t index1,
                       const Orthanc::DicomTag& sequence2,
                       size_t index2,
                       const Orthanc::DicomTag& sequence3,
                       size_t index3,
                       const Orthanc::DicomTag& tag) :
    finalTag_(tag)
  {
    AddIndexedTagToPrefix(sequence1, index1);
    AddIndexedTagToPrefix(sequence2, index2);
    AddIndexedTagToPrefix(sequence3, index3);
  }


  void DicomPath::AddIndexedTagToPrefix(const Orthanc::DicomTag& tag,
                                        size_t index)
  {
    prefix_.push_back(PrefixItem::CreateIndexed(tag, index));
  }


  void DicomPath::AddUniversalTagToPrefix(const Orthanc::DicomTag& tag)
  {
    prefix_.push_back(PrefixItem::CreateUniversal(tag));
  }
  

  std::string DicomPath::Format() const
  {
    std::string s;

    for (size_t i = 0; i < prefix_.size(); i++)
    {
      s += "(" + prefix_[i].GetTag().Format() + ")";

      if (prefix_[i].IsUniversal())
      {
        s += "[*].";
      }
      else
      {
        s += "[" + boost::lexical_cast<std::string>(prefix_[i].GetIndex()) + "].";
      }
    }

    return s + "(" + finalTag_.Format() + ")";
  }

  
  DicomPath DicomPath::Parse(const std::string& s,
                             bool allowUniversal)
  {
    std::vector<std::string> tokens;
    Toolbox::TokenizeString(tokens, s, '.');

    if (tokens.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange, "Empty path to DICOM tags");
    }

    const DicomTag finalTag = ParseTag(Toolbox::StripSpaces(tokens[tokens.size() - 1]));

    DicomPath path(finalTag);

    for (size_t i = 0; i < tokens.size() - 1; i++)
    {
      size_t pos = tokens[i].find('[');
      if (pos == std::string::npos)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "Parent path doesn't contain an index");
      }
      else
      {
        const std::string left = Orthanc::Toolbox::StripSpaces(tokens[i].substr(0, pos));
        const std::string right = Orthanc::Toolbox::StripSpaces(tokens[i].substr(pos + 1));

        if (left.empty())
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange, "Parent path doesn't contain a tag");
        }            
        else if (right.empty() ||
                 right[right.size() - 1] != ']')
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange, "Parent path doesn't contain the end of the index");
        }
        else
        {
          DicomTag tag = ParseTag(left);

          try
          {
            std::string s = Toolbox::StripSpaces(right.substr(0, right.size() - 1));
            if (s == "*")
            {
              if (allowUniversal)
              {
                path.AddUniversalTagToPrefix(tag);
              }
              else
              {
                throw OrthancException(ErrorCode_ParameterOutOfRange, "Cannot create an universal parent path");
              }
            }
            else
            {
              int index = boost::lexical_cast<int>(s);
              if (index < 0)
              {
                throw OrthancException(ErrorCode_ParameterOutOfRange, "Negative index in parent path: " + s);
              }
              else
              {
                path.AddIndexedTagToPrefix(tag, static_cast<size_t>(index));
              }
            }
          }
          catch (boost::bad_lexical_cast&)
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange, "Not a valid index in parent path: [" + right);
          }
        }
      }
    }

    return path;
  }
}
