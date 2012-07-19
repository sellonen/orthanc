/**
 * Palantir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "FromDcmtkBridge.h"

#include "ToDcmtkBridge.h"
#include "DicomIntegerPixelAccessor.h"
#include "../Core/PalantirException.h"
#include "../Core/PngWriter.h"
#include "../Core/DicomFormat/DicomString.h"
#include "../Core/DicomFormat/DicomNullValue.h"

#include <boost/locale.hpp>
#include <boost/lexical_cast.hpp>

#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcvrfd.h>
#include <dcmtk/dcmdata/dcvrfl.h>
#include <dcmtk/dcmdata/dcvrsl.h>
#include <dcmtk/dcmdata/dcvrss.h>
#include <dcmtk/dcmdata/dcvrul.h>
#include <dcmtk/dcmdata/dcvrus.h>

#include <boost/math/special_functions/round.hpp>

namespace Palantir
{
  void FromDcmtkBridge::Convert(DicomMap& target, DcmDataset& dataset)
  {
    target.Clear();
    for (unsigned long i = 0; i < dataset.card(); i++)
    {
      DcmElement* element = dataset.getElement(i);
      if (element && element->isLeaf())
      {
        target.SetValue(element->getTag().getGTag(),
                        element->getTag().getETag(),
                        ConvertLeafElement(*element));
      }
    }
  }


  DicomTag FromDcmtkBridge::GetTag(const DcmElement& element)
  {
    return DicomTag(element.getGTag(), element.getETag());
  }


  DicomValue* FromDcmtkBridge::ConvertLeafElement(DcmElement& element)
  {
    if (!element.isLeaf())
    {
      throw PalantirException("Only applicable to leaf elements");
    }

    if (element.isaString())
    {
      char *c;
      if (element.getString(c).good() &&
          c != NULL)
      {
        std::string s(c);
        std::string utf8;
        try
        {
          utf8 = boost::locale::conv::to_utf<char>(s, "ISO-8859-1"); // TODO Parameter?
        }
        catch (std::runtime_error&)
        {
          // Bad input string or bad encoding
          utf8 = s;
        }

        return new DicomString(utf8);
      }
      else
      {
        return new DicomNullValue;
      }
    }

    try
    {
      // http://support.dcmtk.org/docs/dcvr_8h-source.html
      switch (element.getVR())
      {

        /**
         * TODO.
         **/
    
      case EVR_DS:  // decimal string
      case EVR_IS:  // integer string
      case EVR_OB:  // other byte
      case EVR_OF:  // other float
      case EVR_OW:  // other word
      case EVR_AS:  // age string
      case EVR_AT:  // attribute tag
      case EVR_DA:  // date string
      case EVR_DT:  // date time string
      case EVR_TM:  // time string
      case EVR_UN:  // unknown value representation
        return new DicomNullValue();


        /**
         * String types, should never happen at this point because of
         * "element.isaString()".
         **/
      
      case EVR_AE:  // application entity title
      case EVR_CS:  // code string
      case EVR_SH:  // short string
      case EVR_LO:  // long string
      case EVR_ST:  // short text
      case EVR_LT:  // long text
      case EVR_UT:  // unlimited text
      case EVR_PN:  // person name
      case EVR_UI:  // unique identifier
        return new DicomNullValue();


        /**
         * Numerical types
         **/ 
      
      case EVR_SL:  // signed long
      {
        Sint32 f;
        if (dynamic_cast<DcmSignedLong&>(element).getSint32(f).good())
        {
          return new DicomString(boost::lexical_cast<std::string>(f));
        }
        else
        {
          return new DicomNullValue();
        }
      }

      case EVR_SS:  // signed short
      {
        Sint16 f;
        if (dynamic_cast<DcmSignedShort&>(element).getSint16(f).good())
        {
          return new DicomString(boost::lexical_cast<std::string>(f));
        }
        else
        {
          return new DicomNullValue();
        }
      }

      case EVR_UL:  // unsigned long
      {
        Uint32 f;
        if (dynamic_cast<DcmUnsignedLong&>(element).getUint32(f).good())
        {
          return new DicomString(boost::lexical_cast<std::string>(f));
        }
        else
        {
          return new DicomNullValue();
        }
      }

      case EVR_US:  // unsigned short
      {
        Uint16 f;
        if (dynamic_cast<DcmUnsignedShort&>(element).getUint16(f).good())
        {
          return new DicomString(boost::lexical_cast<std::string>(f));
        }
        else
        {
          return new DicomNullValue();
        }
      }

      case EVR_FL:  // float single-precision
      {
        Float32 f;
        if (dynamic_cast<DcmFloatingPointSingle&>(element).getFloat32(f).good())
        {
          return new DicomString(boost::lexical_cast<std::string>(f));
        }
        else
        {
          return new DicomNullValue();
        }
      }

      case EVR_FD:  // float double-precision
      {
        Float64 f;
        if (dynamic_cast<DcmFloatingPointDouble&>(element).getFloat64(f).good())
        {
          return new DicomString(boost::lexical_cast<std::string>(f));
        }
        else
        {
          return new DicomNullValue();
        }
      }


      /**
       * Sequence types, should never occur at this point because of
       * "element.isLeaf()".
       **/

      case EVR_SQ:  // sequence of items
        return new DicomNullValue;


        /**
         * Internal to DCMTK.
         **/ 

      case EVR_ox:  // OB or OW depending on context
      case EVR_xs:  // SS or US depending on context
      case EVR_lt:  // US, SS or OW depending on context, used for LUT Data (thus the name)
      case EVR_na:  // na="not applicable", for data which has no VR
      case EVR_up:  // up="unsigned pointer", used internally for DICOMDIR suppor
      case EVR_item:  // used internally for items
      case EVR_metainfo:  // used internally for meta info datasets
      case EVR_dataset:  // used internally for datasets
      case EVR_fileFormat:  // used internally for DICOM files
      case EVR_dicomDir:  // used internally for DICOMDIR objects
      case EVR_dirRecord:  // used internally for DICOMDIR records
      case EVR_pixelSQ:  // used internally for pixel sequences in a compressed image
      case EVR_pixelItem:  // used internally for pixel items in a compressed image
      case EVR_UNKNOWN: // used internally for elements with unknown VR (encoded with 4-byte length field in explicit VR)
      case EVR_PixelData:  // used internally for uncompressed pixeld data
      case EVR_OverlayData:  // used internally for overlay data
      case EVR_UNKNOWN2B:  // used internally for elements with unknown VR with 2-byte length field in explicit VR
        return new DicomNullValue;


        /**
         * Default case.
         **/ 

      default:
        return new DicomNullValue;
      }
    }
    catch (boost::bad_lexical_cast)
    {
      return new DicomNullValue;
    }
  }


  static void StoreElement(Json::Value& target,
                           DcmElement& element,
                           unsigned int maxStringLength);

  static void StoreItem(Json::Value& target,
                        DcmItem& item,
                        unsigned int maxStringLength)
  {
    target = Json::Value(Json::objectValue);

    for (unsigned long i = 0; i < item.card(); i++)
    {
      DcmElement* element = item.getElement(i);
      StoreElement(target, *element, maxStringLength);
    }
  }


  static void StoreElement(Json::Value& target,
                           DcmElement& element,
                           unsigned int maxStringLength)
  {
    assert(target.type() == Json::objectValue);

    const std::string tagName = FromDcmtkBridge::GetName(FromDcmtkBridge::GetTag(element));

    if (element.isLeaf())
    {
      std::auto_ptr<DicomValue> v(FromDcmtkBridge::ConvertLeafElement(element));
      if (v->IsNull())
      {
        target[tagName] = Json::nullValue;
      }
      else
      {
        std::string s = v->AsString();
        if (maxStringLength == 0 ||
            s.size() <= maxStringLength)
        {
          target[tagName] = s;
        }
        else
        {
          // An integer value of 0 in JSON indicates too long field
          target[tagName] = 0; 
        }
      }
    }
    else
    {
      target[tagName] = Json::Value(Json::arrayValue);

      // "All subclasses of DcmElement except for DcmSequenceOfItems
      // are leaf nodes, while DcmSequenceOfItems, DcmItem, DcmDataset
      // etc. are not." The following cast is thus OK.
      DcmSequenceOfItems& sequence = dynamic_cast<DcmSequenceOfItems&>(element);

      for (unsigned long i = 0; i < sequence.card(); i++)
      {
        DcmItem* child = sequence.getItem(i);
        Json::Value& v = target[tagName].append(Json::objectValue);
        StoreItem(v, *child, maxStringLength);
      }  
    }
  }


  void FromDcmtkBridge::ToJson(Json::Value& root, 
                               DcmDataset& dataset,
                               unsigned int maxStringLength)
  {
    StoreItem(root, dataset, maxStringLength);
  }



  void FromDcmtkBridge::ToJson(Json::Value& target, 
                               const std::string& path,
                               unsigned int maxStringLength)
  {
    DcmFileFormat dicom;
    if (!dicom.loadFile(path.c_str()).good())
    {
      throw PalantirException(ErrorCode_BadFileFormat);
    }
    else
    {
      FromDcmtkBridge::ToJson(target, *dicom.getDataset(), maxStringLength);
    }
  }


  void FromDcmtkBridge::ExtractNormalizedImage(std::string& result,
                                               DcmDataset& dataset)
  {
    // See also: http://support.dcmtk.org/wiki/dcmtk/howto/accessing-compressed-data

    PngWriter w;
    std::auto_ptr<DicomIntegerPixelAccessor> accessor;

    DicomMap m;
    FromDcmtkBridge::Convert(m, dataset);

    DcmElement* e;
    if (dataset.findAndGetElement(ToDcmtkBridge::Convert(DicomTag::PIXEL_DATA), e).good() &&
        e != NULL)
    {
      Uint8* pixData = NULL;
      if (e->getUint8Array(pixData) == EC_Normal)
      {    
        accessor.reset(new DicomIntegerPixelAccessor(m, pixData, e->getLength()));
      }
    }

    if (accessor.get() == NULL ||
        accessor->GetWidth() == 0 ||
        accessor->GetHeight() == 0)
    {
      w.WriteToMemory(result, 0, 0, 0, PixelFormat_Grayscale8, NULL);
    }
    else
    {
      int32_t min, max;
      accessor->GetExtremeValues(min, max);

      std::vector<uint8_t> image(accessor->GetWidth() * accessor->GetHeight(), 0);
      if (min != max)
      {
        uint8_t* result = &image[0];
        for (unsigned int y = 0; y < accessor->GetHeight(); y++)
        {
          for (unsigned int x = 0; x < accessor->GetWidth(); x++, result++)
          {
            int32_t v = accessor->GetValue(x, y);
            *result = static_cast<uint8_t>(boost::math::lround(static_cast<float>(v - min) / static_cast<float>(max - min) * 255.0f));
          }
        }
      }

      w.WriteToMemory(result, accessor->GetWidth(), accessor->GetHeight(),
                      accessor->GetWidth(), PixelFormat_Grayscale8, &image[0]);
    }
  }


  void FromDcmtkBridge::ExtractNormalizedImage(std::string& result,
                                               const std::string& dicomContent)
  {
    DcmInputBufferStream is;
    if (dicomContent.size() > 0)
    {
      is.setBuffer(&dicomContent[0], dicomContent.size());
    }
    is.setEos();

    DcmFileFormat dicom;
    if (dicom.read(is).good())
    {
      ExtractNormalizedImage(result, *dicom.getDataset());
    }
    else
    {
      throw PalantirException(ErrorCode_BadFileFormat);
    }
  }



  std::string FromDcmtkBridge::GetName(const DicomTag& t)
  {
    DcmTagKey tag(t.GetGroup(), t.GetElement());
    const DcmDataDictionary& dict = dcmDataDict.rdlock();
    const DcmDictEntry* entry = dict.findEntry(tag, NULL);

    std::string s("Unknown");
    if (entry != NULL)
    {
      s = std::string(entry->getTagName());
    }

    dcmDataDict.unlock();
    return s;
  }


  DicomTag FromDcmtkBridge::FindTag(const char* name)
  {
    const DcmDataDictionary& dict = dcmDataDict.rdlock();
    const DcmDictEntry* entry = dict.findEntry(name);

    if (entry == NULL)
    {
      dcmDataDict.unlock();
      throw PalantirException("Unknown DICOM tag");
    }
    else
    {
      DcmTagKey key = entry->getKey();
      DicomTag tag(key.getGroup(), key.getElement());
      dcmDataDict.unlock();
      return tag;
    }
  }


  void FromDcmtkBridge::Print(FILE* fp, const DicomMap& m)
  {
    for (DicomMap::Map::const_iterator 
           it = m.map_.begin(); it != m.map_.end(); it++)
    {
      DicomTag t = it->first;
      std::string s = it->second->AsString();
      printf("0x%04x 0x%04x (%s) [%s]\n", t.GetGroup(), t.GetElement(), GetName(t).c_str(), s.c_str());
    }
  }


  void FromDcmtkBridge::ToJson(Json::Value& result,
                               const DicomMap& values)
  {
    if (result.type() != Json::objectValue)
    {
      throw PalantirException(ErrorCode_BadParameterType);
    }

    result.clear();

    for (DicomMap::Map::const_iterator 
           it = values.map_.begin(); it != values.map_.end(); it++)
    {
      result[GetName(it->first)] = it->second->AsString();
    }
  }
}
