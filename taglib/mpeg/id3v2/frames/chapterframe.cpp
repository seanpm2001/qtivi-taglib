/***************************************************************************
    copyright            : (C) 2013 by Lukas Krejci
    email                : krejclu6@fel.cvut.cz
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include <tbytevectorlist.h>
#include <tpropertymap.h>
#include <tdebug.h>
#include <stdio.h>

#include "chapterframe.h"

using namespace TagLib;
using namespace ID3v2;

class ChapterFrame::ChapterFramePrivate
{
public:
  ChapterFramePrivate() :
    tagHeader(0)
  {
    embeddedFrameList.setAutoDelete(true);
  }

  const ID3v2::Header *tagHeader;
  ByteVector elementID;
  uint startTime;
  uint endTime;
  uint startOffset;
  uint endOffset;
  FrameListMap embeddedFrameListMap;
  FrameList embeddedFrameList;
};

////////////////////////////////////////////////////////////////////////////////
// public methods
////////////////////////////////////////////////////////////////////////////////

ChapterFrame::ChapterFrame(const ID3v2::Header *tagHeader, const ByteVector &data) :
    ID3v2::Frame(data)
{
  d = new ChapterFramePrivate;
  d->tagHeader = tagHeader;
  setData(data);
}

ChapterFrame::ChapterFrame(const ByteVector &eID, const uint &sT, const uint &eT,
                           const uint &sO, const uint &eO, const FrameList &eF) :
    ID3v2::Frame("CHAP")
{
  d = new ChapterFramePrivate;
  d->elementID = eID;
  d->startTime = sT;
  d->endTime = eT;
  d->startOffset = sO;
  d->endOffset = eO;
  FrameList l = eF;
  for(FrameList::ConstIterator it = l.begin(); it != l.end(); ++it)
    addEmbeddedFrame(*it);
}

ChapterFrame::~ChapterFrame()
{
  delete d;
}

ByteVector ChapterFrame::elementID() const
{
  return d->elementID;
}

uint ChapterFrame::startTime() const
{
  return d->startTime;
}

uint ChapterFrame::endTime() const
{
  return d->endTime;
}

uint ChapterFrame::startOffset() const
{
  return d->startOffset;
}

uint ChapterFrame::endOffset() const
{
  return d->endOffset;
}

void ChapterFrame::setElementID(const ByteVector &eID)
{
  d->elementID = eID;
  if(eID.at(eID.size() - 1) != char(0))
    d->elementID.append(char(0));
}

void ChapterFrame::setStartTime(const uint &sT)
{
  d->startTime = sT;
}

void ChapterFrame::setEndTime(const uint &eT)
{
  d->endTime = eT;
}

void ChapterFrame::setStartOffset(const uint &sO)
{
  d->startOffset = sO;
}

void ChapterFrame::setEndOffset(const uint &eO)
{
  d->endOffset = eO;
}

const FrameListMap &ChapterFrame::embeddedFrameListMap() const
{
  return d->embeddedFrameListMap;
}

const FrameList &ChapterFrame::embeddedFrameList() const
{
  return d->embeddedFrameList;
}

const FrameList &ChapterFrame::embeddedFrameList(const ByteVector &frameID) const
{
  return d->embeddedFrameListMap[frameID];
}

void ChapterFrame::addEmbeddedFrame(Frame *frame)
{
  d->embeddedFrameList.append(frame);
  d->embeddedFrameListMap[frame->frameID()].append(frame);
}

void ChapterFrame::removeEmbeddedFrame(Frame *frame, bool del)
{
  // remove the frame from the frame list
  FrameList::Iterator it = d->embeddedFrameList.find(frame);
  d->embeddedFrameList.erase(it);

  // ...and from the frame list map
  it = d->embeddedFrameListMap[frame->frameID()].find(frame);
  d->embeddedFrameListMap[frame->frameID()].erase(it);

  // ...and delete as desired
  if(del)
    delete frame;
}

void ChapterFrame::removeEmbeddedFrames(const ByteVector &id)
{
  FrameList l = d->embeddedFrameListMap[id];
  for(FrameList::ConstIterator it = l.begin(); it != l.end(); ++it)
    removeEmbeddedFrame(*it, true);
}

String ChapterFrame::toString() const
{
  return String::null;
}

PropertyMap ChapterFrame::asProperties() const
{
  PropertyMap map;

  map.unsupportedData().append(frameID() + String("/") + d->elementID);

  return map;
}

ChapterFrame *ChapterFrame::findByElementID(const ID3v2::Tag *tag, const ByteVector &eID) // static
{
  ID3v2::FrameList comments = tag->frameList("CHAP");

  for(ID3v2::FrameList::ConstIterator it = comments.begin();
      it != comments.end();
      ++it)
  {
    ChapterFrame *frame = dynamic_cast<ChapterFrame *>(*it);
    if(frame && frame->elementID() == eID)
      return frame;
  }

  return 0;
}

void ChapterFrame::parseFields(const ByteVector &data)
{
  size_t size = data.size();
  if(size < 18) {
    debug("A CHAP frame must contain at least 18 bytes (1 byte element ID "
          "terminated by null and 4x4 bytes for start and end time and offset).");
    return;
  }

  size_t pos = 0;
  size_t embPos = 0;
  d->elementID = readStringField(data, String::Latin1, pos).data(String::Latin1);
  d->elementID.append(char(0));
  d->startTime = data.toUInt32BE(pos);
  pos += 4;
  d->endTime = data.toUInt32BE(pos);
  pos += 4;
  d->startOffset = data.toUInt32BE(pos);
  pos += 4;
  d->endOffset = data.toUInt32BE(pos);
  pos += 4;
  size -= pos;
  while(embPos < size - header()->size()) {
    Frame *frame = FrameFactory::instance()->createFrame(data.mid(pos + embPos), d->tagHeader);

    if(!frame)
      return;

    // Checks to make sure that frame parsed correctly.
    if(frame->size() <= 0) {
      delete frame;
      return;
    }

    embPos += frame->size() + header()->size();
    addEmbeddedFrame(frame);
  }
}

ByteVector ChapterFrame::renderFields() const
{
  ByteVector data;

  data.append(d->elementID);
  data.append(ByteVector::fromUInt32BE(d->startTime));
  data.append(ByteVector::fromUInt32BE(d->endTime));
  data.append(ByteVector::fromUInt32BE(d->startOffset));
  data.append(ByteVector::fromUInt32BE(d->endOffset));
  FrameList l = d->embeddedFrameList;
  for(FrameList::ConstIterator it = l.begin(); it != l.end(); ++it)
    data.append((*it)->render());

  return data;
}

ChapterFrame::ChapterFrame(const ID3v2::Header *tagHeader, const ByteVector &data, Header *h) :
  Frame(h)
{
  d = new ChapterFramePrivate;
  d->tagHeader = tagHeader;
  parseFields(fieldData(data));
}
