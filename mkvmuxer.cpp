// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "mkvmuxer.hpp"
#include "mkvmuxerutil.hpp"
#include "webmids.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <new>
#include <string.h>
#include <time.h>

namespace mkvmuxer {

IMkvWriter::IMkvWriter() {
}

IMkvWriter::~IMkvWriter() {
}

bool WriteEbmlHeader(IMkvWriter* pWriter) {
  // Level 0
  uint64 size = EbmlElementSize(kMkvEBMLVersion, 1ULL, false);
  size += EbmlElementSize(kMkvEBMLReadVersion, 1ULL, false);
  size += EbmlElementSize(kMkvEBMLMaxIDLength, 4ULL, false);
  size += EbmlElementSize(kMkvEBMLMaxSizeLength, 8ULL, false);
  size += EbmlElementSize(kMkvDocType, "webm", false);
  size += EbmlElementSize(kMkvDocTypeVersion, 2ULL, false);
  size += EbmlElementSize(kMkvDocTypeReadVersion, 2ULL, false);

  if (!WriteEbmlMasterElement(pWriter, kMkvEBML, size))
    return false;

  if (!WriteEbmlElement(pWriter, kMkvEBMLVersion, 1ULL))
    return false;
  if (!WriteEbmlElement(pWriter, kMkvEBMLReadVersion, 1ULL))
    return false;
  if (!WriteEbmlElement(pWriter, kMkvEBMLMaxIDLength, 4ULL))
    return false;
  if (!WriteEbmlElement(pWriter, kMkvEBMLMaxSizeLength, 8ULL))
    return false;
  if (!WriteEbmlElement(pWriter, kMkvDocType, "webm"))
    return false;
  if (!WriteEbmlElement(pWriter, kMkvDocTypeVersion, 2ULL))
    return false;
  if (!WriteEbmlElement(pWriter, kMkvDocTypeReadVersion, 2ULL))
    return false;

  return true;
}

Frame::Frame()
: frame_(NULL),
  length_(0),
  track_number_(0),
  timestamp_(0),
  is_key_(false) {
}

Frame::~Frame() {
  delete [] frame_;
}

bool Frame::Init(const uint8* frame, uint64 length) {
  uint8* data = new (std::nothrow) uint8[static_cast<unsigned int>(length)];
  if (!data)
    return false;

  delete [] frame_;
  frame_ = data;
  length_ = length;

  memcpy(frame_, frame, static_cast<size_t>(length_));
  return true;
}

CuePoint::CuePoint()
: time_(0),
  track_(0),
  cluster_pos_(0),
  block_number_(1),
  output_block_number_(true) {
}

CuePoint::~CuePoint() {
}

bool CuePoint::Write(IMkvWriter* writer) const {
  assert(writer);
  assert(track_ > 0);
  assert(cluster_pos_ > 0);

  uint64 size = EbmlElementSize(kMkvCueClusterPosition, cluster_pos_, false);
  size += EbmlElementSize(kMkvCueTrack, track_, false);
  if (output_block_number_ && block_number_ > 1)
    size += EbmlElementSize(kMkvCueBlockNumber, block_number_, false);
  uint64 track_pos_size = EbmlElementSize(kMkvCueTrackPositions, size, true) +
                          size;
  uint64 payload_size = EbmlElementSize(kMkvCueTime, time_, false) +
                        track_pos_size;

  if (!WriteEbmlMasterElement(writer, kMkvCuePoint, payload_size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvCueTime, time_))
    return false;

  if (!WriteEbmlMasterElement(writer, kMkvCueTrackPositions, size))
    return false;
  if (!WriteEbmlElement(writer, kMkvCueTrack, track_))
    return false;
  if (!WriteEbmlElement(writer, kMkvCueClusterPosition, cluster_pos_))
    return false;
  if (output_block_number_ && block_number_ > 1)
    if (!WriteEbmlElement(writer, kMkvCueBlockNumber, block_number_))
      return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;
  assert(stop_position - payload_position == payload_size);

  return true;
}

uint64 CuePoint::PayloadSize() const {
  uint64 size = EbmlElementSize(kMkvCueClusterPosition, cluster_pos_, false);
  size += EbmlElementSize(kMkvCueTrack, track_, false);
  if (output_block_number_ && block_number_ > 1)
    size += EbmlElementSize(kMkvCueBlockNumber, block_number_, false);
  uint64 track_pos_size = EbmlElementSize(kMkvCueTrackPositions, size, true) +
                          size;
  uint64 payload_size = EbmlElementSize(kMkvCueTime, time_, false) +
                        track_pos_size;

  return payload_size;
}

uint64 CuePoint::Size() const {
  const uint64 payload_size = PayloadSize();
  uint64 element_size = EbmlElementSize(kMkvCuePoint, payload_size, true) +
                        payload_size;

  return element_size;
}

Cues::Cues()
: cue_entries_capacity_(0),
  cue_entries_size_(0),
  cue_entries_(NULL),
  output_block_number_(true) {
}

Cues::~Cues() {
  if (cue_entries_) {
    for (int i=0; i<cue_entries_size_; ++i) {
      CuePoint* const cue = cue_entries_[i];
      delete cue;
    }
    delete [] cue_entries_;
  }
}

bool Cues::AddCue(CuePoint* cue) {
  assert(cue);

  if ((cue_entries_size_ + 1) > cue_entries_capacity_) {
    // Add more CuePoints.
    const int new_capacity =
      (!cue_entries_capacity_)? 2 : cue_entries_capacity_*2;

    assert(new_capacity > 0);
    CuePoint** cues = new (std::nothrow) CuePoint*[new_capacity];
    if (!cues)
      return false;

    for (int i=0; i<cue_entries_size_; ++i) {
      cues[i] = cue_entries_[i];
    }

    delete [] cue_entries_;

    cue_entries_ = cues;
    cue_entries_capacity_ = new_capacity;
  }

  cue->output_block_number(output_block_number_);
  cue_entries_[cue_entries_size_++] = cue;
  return true;
}

// Returns the track by index. Returns NULL if there is no track match.
const CuePoint* Cues::GetCueByIndex(int index) const {
  if (cue_entries_ == NULL)
    return NULL;

  if (index >= cue_entries_size_)
    return NULL;

  return cue_entries_[index];
}

bool Cues::Write(IMkvWriter* writer) const {
  assert(writer);

  uint64 size = 0;
  for (int i=0; i<cue_entries_size_; ++i) {
    const CuePoint* cue = GetCueByIndex(i);
    assert(cue);
    size += cue->Size();
  }

  if (!WriteEbmlMasterElement(writer, kMkvCues, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  for (int i=0; i<cue_entries_size_; ++i) {
    const CuePoint* cue = GetCueByIndex(i);
    assert(cue);
    if (!cue->Write(writer))
      return false;
  }

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;
  assert(stop_position - payload_position == size);

  return true;
}

VideoTrack::VideoTrack()
    : width_(0),
      height_(0),
      display_width_(0),
      display_height_(0),
      frame_rate_(0.0),
      stereo_mode_(0) {
}

VideoTrack::~VideoTrack() {
}

bool VideoTrack::SetStereoMode(uint64 stereo_mode) {
  if (stereo_mode != 0 &&
      stereo_mode != 1 &&
      stereo_mode != 2 &&
      stereo_mode != 3 &&
      stereo_mode != 11)
    return false;

  stereo_mode_ = stereo_mode;
  return true;
}

uint64 VideoTrack::Size() const {
  const uint64 parent_size = Track::Size();

  uint64 size = VideoPayloadSize();
  size += EbmlElementSize(kMkvVideo, size, true);

  return parent_size + size;
}

uint64 VideoTrack::PayloadSize() const {
  const uint64 parent_size = Track::PayloadSize();

  uint64 size = VideoPayloadSize();
  size += EbmlElementSize(kMkvVideo, size, true);

  return parent_size + size;
}

bool VideoTrack::Write(IMkvWriter* writer) const {
  assert(writer);

  if (!Track::Write(writer))
    return false;

  uint64 size = VideoPayloadSize();

  if (!WriteEbmlMasterElement(writer, kMkvVideo, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvPixelWidth, width_))
    return false;
  if (!WriteEbmlElement(writer, kMkvPixelHeight, height_))
    return false;
  if (display_width_ > 0)
    if (!WriteEbmlElement(writer, kMkvDisplayWidth, display_width_))
      return false;
  if (display_height_ > 0)
    if (!WriteEbmlElement(writer, kMkvDisplayHeight, display_height_))
      return false;
  if (stereo_mode_ > 0)
    if (!WriteEbmlElement(writer, kMkvStereoMode, stereo_mode_))
      return false;
  if (frame_rate_ > 0.0)
    if (!WriteEbmlElement(writer,
                          kMkvFrameRate,
                          static_cast<float>(frame_rate_)))
      return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;
  assert(stop_position - payload_position == size);

  return true;
}

uint64 VideoTrack::VideoPayloadSize() const {
  uint64 size = EbmlElementSize(kMkvPixelWidth, width_, false);
  size += EbmlElementSize(kMkvPixelHeight, height_, false);
  if (display_width_ > 0)
    size += EbmlElementSize(kMkvDisplayWidth, display_width_, false);
  if (display_height_ > 0)
    size += EbmlElementSize(kMkvDisplayHeight, display_height_, false);
  if (stereo_mode_ > 0)
    size += EbmlElementSize(kMkvStereoMode, stereo_mode_, false);
  if (frame_rate_ > 0.0)
    size += EbmlElementSize(kMkvFrameRate,
                            static_cast<float>(frame_rate_),
                            false);

  return size;
}

AudioTrack::AudioTrack()
: bit_depth_(0),
  channels_(1),
  sample_rate_(0.0) {
}

AudioTrack::~AudioTrack() {
}

uint64 AudioTrack::Size() const {
  const uint64 parent_size = Track::Size();

  uint64 size = EbmlElementSize(kMkvSamplingFrequency,
                                static_cast<float>(sample_rate_),
                                false);
  size += EbmlElementSize(kMkvChannels, channels_, false);
  if (bit_depth_ > 0)
    size += EbmlElementSize(kMkvBitDepth, bit_depth_, false);
  size += EbmlElementSize(kMkvAudio, size, true);

  return parent_size + size;
}

uint64 AudioTrack::PayloadSize() const {
  const uint64 parent_size = Track::PayloadSize();

  uint64 size = EbmlElementSize(kMkvSamplingFrequency,
                                static_cast<float>(sample_rate_),
                                false);
  size += EbmlElementSize(kMkvChannels, channels_, false);
  if (bit_depth_ > 0)
    size += EbmlElementSize(kMkvBitDepth, bit_depth_, false);
  size += EbmlElementSize(kMkvAudio, size, true);

  return parent_size + size;
}

bool AudioTrack::Write(IMkvWriter* writer) const {
  assert(writer);

  if (!Track::Write(writer))
    return false;

  // Calculate AudioSettings size.
  uint64 size = EbmlElementSize(kMkvSamplingFrequency,
                                static_cast<float>(sample_rate_),
                                false);
  size += EbmlElementSize(kMkvChannels, channels_, false);
  if (bit_depth_ > 0)
    size += EbmlElementSize(kMkvBitDepth, bit_depth_, false);

  if (!WriteEbmlMasterElement(writer, kMkvAudio, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer,
                        kMkvSamplingFrequency,
                        static_cast<float>(sample_rate_)))
    return false;
  if (!WriteEbmlElement(writer, kMkvChannels, channels_))
    return false;
  if (bit_depth_ > 0)
    if (!WriteEbmlElement(writer, kMkvBitDepth, bit_depth_))
      return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;
  assert(stop_position - payload_position == size);

  return true;
}

Track::Track()
    : codec_id_(NULL),
      codec_private_(NULL),
      language_(NULL),
      name_(NULL),
      number_(0),
      type_(0),
      uid_(MakeUID()),
      codec_private_length_(0) {
}

Track::~Track() {
  if (codec_id_) {
    delete [] codec_id_;
    codec_id_ = NULL;
  }

  if (codec_private_) {
    delete [] codec_private_;
    codec_private_ = NULL;
  }
}

uint64 Track::Size() const {
  uint64 size = Track::PayloadSize();
  size += EbmlElementSize(kMkvTrackEntry, size, true);

  return size;
}

uint64 Track::PayloadSize() const {
  uint64 size = EbmlElementSize(kMkvTrackNumber, number_, false);
  size += EbmlElementSize(kMkvTrackUID, uid_, false);
  size += EbmlElementSize(kMkvTrackType, type_, false);
  if (codec_id_)
    size += EbmlElementSize(kMkvCodecID, codec_id_, false);
  if (codec_private_)
    size += EbmlElementSize(kMkvCodecPrivate,
                            codec_private_,
                            codec_private_length_,
                            false);
  if (language_)
    size += EbmlElementSize(kMkvLanguage, language_, false);
  if (name_)
    size += EbmlElementSize(kMkvName, name_, false);

  return size;
}

bool Track::Write(IMkvWriter* writer) const {
  assert(writer);

  // |size| may be bigger than what is written out in this function because
  // derived classes may write out more data in the Track element.
  const uint64 size = PayloadSize();

  if (!WriteEbmlMasterElement(writer, kMkvTrackEntry, size))
    return false;

  uint64 test = EbmlElementSize(kMkvTrackNumber, number_, false);
  test += EbmlElementSize(kMkvTrackUID, uid_, false);
  test += EbmlElementSize(kMkvTrackType, type_, false);
  if (codec_id_)
    test += EbmlElementSize(kMkvCodecID, codec_id_, false);
  if (codec_private_)
    test += EbmlElementSize(kMkvCodecPrivate,
                            codec_private_,
                            codec_private_length_,
                            false);
  if (language_)
    test += EbmlElementSize(kMkvLanguage, language_, false);
  if (name_)
    test += EbmlElementSize(kMkvName, name_, false);

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvTrackNumber, number_))
    return false;
  if (!WriteEbmlElement(writer, kMkvTrackUID, uid_))
    return false;
  if (!WriteEbmlElement(writer, kMkvTrackType, type_))
    return false;
  if (codec_id_) {
    if (!WriteEbmlElement(writer, kMkvCodecID, codec_id_))
      return false;
  }
  if (codec_private_) {
    if (!WriteEbmlElement(writer,
                          kMkvCodecPrivate,
                          codec_private_,
                          codec_private_length_))
      return false;
  }
  if (language_) {
    if (!WriteEbmlElement(writer, kMkvLanguage, language_))
      return false;
  }
  if (name_) {
    if (!WriteEbmlElement(writer, kMkvName, name_))
      return false;
  }

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;
  assert(stop_position - payload_position == test);

  return true;
}

bool Track::SetCodecPrivate(const uint8* codec_private, uint64 length) {
  assert(codec_private);
  assert(length > 0);

  delete [] codec_private_;

  codec_private_ =
    new (std::nothrow) uint8[static_cast<unsigned long>(length)];
  if (!codec_private_)
    return false;

  memcpy(codec_private_, codec_private, static_cast<size_t>(length));
  codec_private_length_ = length;

  return true;
}

void Track::codec_id(const char* codec_id) {
  assert(codec_id);

  if (codec_id_)
    delete [] codec_id_;

  int length = strlen(codec_id) + 1;
  codec_id_ = new (std::nothrow) char[length];
  if (codec_id_) {
#ifdef WIN32
    strcpy_s(codec_id_, length, codec_id);
#else
    strcpy(codec_id_, codec_id);
#endif
  }
}

// TODO: Vet the language parameter.
void Track::language(const char* language) {
  assert(language);

  if (language_)
    delete [] language_;

  int length = strlen(language) + 1;
  language_ = new (std::nothrow) char[length];
  if (language_) {
#ifdef WIN32
    strcpy_s(language_, length, language);
#else
    strcpy(language_, language);
#endif
  }
}

void Track::name(const char* name) {
  assert(name);

  if (name_)
    delete [] name_;

  int length = strlen(name) + 1;
  name_ = new (std::nothrow) char[length];
  if (name_) {
#ifdef WIN32
    strcpy_s(name_, length, name);
#else
    strcpy(name_, name);
#endif
  }
}

bool Track::is_seeded_ = false;

uint64 Track::MakeUID() {
  uint64 track_uid = 0;

  if (!is_seeded_) {
    srand(static_cast<unsigned int>(time(NULL)));
    is_seeded_ = true;
  }

  for (int i = 0; i < 7; ++i) {  // avoid problems with 8-byte values
    track_uid <<= 8;

    const int nn = rand();
    const int n = 0xFF & (nn >> 4);  // throw away low-order bits

    track_uid |= n;
  }

  return track_uid;
}

Tracks::Tracks()
    : m_trackEntries(NULL),
      m_trackEntriesSize(0) {
}

Tracks::~Tracks() {
  if (m_trackEntries) {
    for (unsigned int i=0; i<m_trackEntriesSize; ++i) {
      Track* const pTrack = m_trackEntries[i];
      delete pTrack;
    }
    delete [] m_trackEntries;
  }
}

bool Tracks::AddTrack(Track* track) {
  const unsigned int count = m_trackEntriesSize+1;

  Track** track_entries = new (std::nothrow) Track*[count];
  if (!track_entries)
    return false;

  for (unsigned int i=0; i<m_trackEntriesSize; ++i) {
    track_entries[i] = m_trackEntries[i];
  }

  delete [] m_trackEntries;

  track->number(count);

  m_trackEntries = track_entries;
  m_trackEntries[m_trackEntriesSize] = track;
  m_trackEntriesSize = count;
  return true;
}

int Tracks::GetTracksCount() const {
  return m_trackEntriesSize;
}

Track* Tracks::GetTrackByNumber(uint64 tn) {
  const int count = GetTracksCount();
  for (int i=0; i<count; ++i) {
    if (m_trackEntries[i]->number() == tn)
      return m_trackEntries[i];
  }

  return NULL;
}

const Track* Tracks::GetTrackByIndex(unsigned long index) const {
  if (m_trackEntries == NULL)
    return NULL;

  if (index >= m_trackEntriesSize)
    return NULL;

  return m_trackEntries[index];
}

bool Tracks::TrackIsAudio(uint64 track_number) {
  Track* track = GetTrackByNumber(track_number);

  if (track->type() == kAudio)
    return true;

  return false;
}


bool Tracks::TrackIsVideo(uint64 track_number) {
  Track* track = GetTrackByNumber(track_number);

  if (track->type() == kVideo)
    return true;

  return false;
}

bool Tracks::Write(IMkvWriter* writer) const {
  assert(writer);

  uint64 size = 0;
  const int count = GetTracksCount();
  for (int i=0; i<count; ++i) {
    const Track* pTrack = GetTrackByIndex(i);
    assert(pTrack);
    size += pTrack->Size();
  }

  if (!WriteEbmlMasterElement(writer, kMkvTracks, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  for (int i=0; i<count; ++i) {
    const Track* pTrack = GetTrackByIndex(i);
    if (!pTrack->Write(writer))
      return false;
  }

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;
  assert(stop_position - payload_position == size);

  return true;
}

Cluster::Cluster(uint64 timecode, IMkvWriter* writer)
: blocks_added_(0),
  timecode_(timecode),
  writer_(writer),
  finalized_(false),
  header_written_(false),
  payload_size_(0),
  size_position_(-1) {
}

Cluster::~Cluster() {
}

bool Cluster::AddFrame(const uint8* frame,
                       uint64 length,
                       uint64 track_number,
                       short timecode,
                       bool is_key) {
  if (finalized_)
    return false;

  if (!header_written_)
    if (!WriteClusterHeader())
      return false;

  const uint64 element_size = WriteSimpleBlock(writer_,
                                               frame,
                                               length,
                                               static_cast<char>(track_number),
                                               timecode,
                                               is_key);
  if (!element_size)
    return false;

  AddPayloadSize(element_size);
  blocks_added_++;

  return true;
}

void Cluster::AddPayloadSize(uint64 size) {
  payload_size_ += size;
}

bool Cluster::Finalize() {
  if (finalized_)
    return false;

  assert(size_position_ != -1);

  if (writer_->Seekable()) {
    const int64 pos = writer_->Position();

    if (writer_->Position(size_position_))
      return false;

    if (WriteUIntSize(writer_, payload_size(), 8))
      return false;

    if (writer_->Position(pos))
      return false;
  }

  finalized_ = true;

  return true;
}

bool Cluster::WriteClusterHeader() {
  assert(!finalized_);

  if (SerializeInt(writer_, kMkvCluster, 4))
    return false;

  // Save for later.
  size_position_ = writer_->Position();

  // Write "unknown" (-1) as cluster size value. We need to write 8 bytes
  // because we do not know how big our cluster will be.
  if (SerializeInt(writer_, 0x01FFFFFFFFFFFFFFULL, 8))
    return false;

  if (!WriteEbmlElement(writer_, kMkvTimecode, timecode()))
    return false;
  AddPayloadSize(EbmlElementSize(kMkvTimecode, timecode(), false));
  header_written_ = true;

  return true;
}

SeekHead::SeekHead()
    : start_pos_(0ULL) {
  for (int i=0; i<kSeekEntryCount; ++i) {
    seek_entry_id_[i] = 0;
    seek_entry_pos_[i] = 0;
  }
}

SeekHead::~SeekHead() {
}

bool SeekHead::Finalize(IMkvWriter* writer) const {
  if (writer->Seekable()) {
    assert(start_pos_ != -1);

    uint64 payload_size = 0;
    uint64 entry_size[kSeekEntryCount];

    for (int i=0; i<kSeekEntryCount; ++i) {
      if (seek_entry_id_[i] != 0) {
        entry_size[i] = EbmlElementSize(
            kMkvSeekID,
            static_cast<uint64>(seek_entry_id_[i]),
            false);
        entry_size[i] += EbmlElementSize(kMkvSeekPosition,
                                         seek_entry_pos_[i],
                                         false);

        payload_size += EbmlElementSize(kMkvSeek, entry_size[i], true) +
                        entry_size[i];
      }
    }

    // No SeekHead elements
    if (payload_size == 0)
      return true;

    const int64 pos = writer->Position();
    if (writer->Position(start_pos_))
      return false;

    if (!WriteEbmlMasterElement(writer, kMkvSeekHead, payload_size))
      return false;

    for (int i=0; i<kSeekEntryCount; ++i) {
      if (seek_entry_id_[i] != 0) {
        if (!WriteEbmlMasterElement(writer, kMkvSeek, entry_size[i]))
          return false;

        if (!WriteEbmlElement(writer,
            kMkvSeekID,
            static_cast<uint64>(seek_entry_id_[i])))
          return false;

        if (!WriteEbmlElement(writer, kMkvSeekPosition, seek_entry_pos_[i]))
          return false;
      }
    }

    const uint64 total_entry_size = kSeekEntryCount * MaxEntrySize();
    const uint64 total_size = EbmlElementSize(kMkvSeekHead,
                                              total_entry_size,
                                              true) + total_entry_size;
    const int64 size_left = total_size - (writer->Position() - start_pos_);

    const uint64 bytes_written = WriteVoidElement(writer, size_left);
    if (!bytes_written)
      return false;

    if (writer->Position(pos))
      return false;
  }

  return true;
}

bool SeekHead::Write(IMkvWriter* writer) {
  const uint64 entry_size = kSeekEntryCount * MaxEntrySize();
  const uint64 size = EbmlElementSize(kMkvSeekHead, entry_size, true);

  start_pos_ = writer->Position();

  const uint64 bytes_written = WriteVoidElement(writer, size + entry_size);
  if (!bytes_written)
    return false;

  return true;
}

bool SeekHead::AddSeekEntry(unsigned long id, uint64 pos) {
  for (int i=0; i<kSeekEntryCount; ++i) {
    if (seek_entry_id_[i] == 0) {
      seek_entry_id_[i] = id;
      seek_entry_pos_[i] = pos;
      return true;
    }
  }
  return false;
}

uint64 SeekHead::MaxEntrySize() const {
  const uint64 max_entry_payload_size =
      EbmlElementSize(kMkvSeekID, 0xffffffffULL, false) +
      EbmlElementSize(kMkvSeekPosition, 0xffffffffffffffffULL, false);
  const uint64 max_entry_size =
      EbmlElementSize(kMkvSeek, max_entry_payload_size, true) +
      max_entry_payload_size;

  return max_entry_size;
}

SegmentInfo::SegmentInfo()
    : timecode_scale_(1000000ULL),
      duration_(-1.0),
      muxing_app_(NULL),
      writing_app_(NULL),
      duration_pos_(-1) {
}

SegmentInfo::~SegmentInfo() {
  if (muxing_app_) {
    delete [] muxing_app_;
    muxing_app_ = NULL;
  }

  if (writing_app_) {
    delete [] writing_app_;
    writing_app_ = NULL;
  }
}

bool SegmentInfo::Init() {
  int major;
  int minor;
  int build;
  int revision;
  GetVersion(major, minor, build, revision);
  char temp[256];
#ifdef WIN32
  sprintf_s(temp, 64, "libwebm-%d.%d.%d.%d", major, minor, build, revision);
#else
  sprintf(temp, "libwebm-%d.%d.%d.%d", major, minor, build, revision);
#endif

  const int app_len = strlen(temp);

  if (muxing_app_)
    delete [] muxing_app_;

  muxing_app_ = new (std::nothrow) char[app_len + 1];
  if (!muxing_app_)
    return false;

#ifdef WIN32
  strcpy_s(muxing_app_, app_len + 1, temp);
#else
  strcpy(muxing_app_, temp);
#endif

  writing_app(temp);
  if (!writing_app_)
    return false;
  return true;
}

bool SegmentInfo::Finalize(IMkvWriter* writer) const {
  assert(writer);

  if (duration_ > 0.0) {
    if (writer->Seekable()) {
      assert(duration_pos_ != -1);

      const int64 pos = writer->Position();

      if (writer->Position(duration_pos_))
        return false;

      if (!WriteEbmlElement(writer,
                            kMkvDuration,
                            static_cast<float>(duration_)))
        return false;

      if (writer->Position(pos))
        return false;
    }
  }

  return true;
}

bool SegmentInfo::Write(IMkvWriter* writer) {
  assert(writer);

  if (!muxing_app_ || !writing_app_)
    return false;

  uint64 size = EbmlElementSize(kMkvTimecodeScale, timecode_scale_, false);
  if (duration_ > 0.0)
    size += EbmlElementSize(kMkvDuration,
                            static_cast<float>(duration_),
                            false);
  size += EbmlElementSize(kMkvMuxingApp, muxing_app_, false);
  size += EbmlElementSize(kMkvWritingApp, writing_app_, false);

  if (!WriteEbmlMasterElement(writer, kMkvInfo, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvTimecodeScale, timecode_scale_))
    return false;

  if (duration_ > 0.0) {
    // Save for later
    duration_pos_ = writer->Position();

    if (!WriteEbmlElement(writer, kMkvDuration, static_cast<float>(duration_)))
      return false;
  }

  if (!WriteEbmlElement(writer, kMkvMuxingApp, muxing_app_))
    return false;
  if (!WriteEbmlElement(writer, kMkvWritingApp, writing_app_))
    return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;
  assert(stop_position - payload_position == size);

  return true;
}

void SegmentInfo::writing_app(const char* app) {
  assert(app);

  if (writing_app_)
    delete [] writing_app_;

  int length = strlen(app) + 1;
  writing_app_ = new (std::nothrow) char[length];
  if (writing_app_) {
#ifdef WIN32
    strcpy_s(writing_app_, length, app);
#else
    strcpy(writing_app_, app);
#endif
  }
}

Segment::Segment(IMkvWriter* writer)
: writer_(writer),
  cluster_list_size_(0),
  cluster_list_capacity_(0),
  cluster_list_(NULL),
  has_video_(false),
  header_written_(false),
  new_cluster_(true),
  new_cuepoint_(false),
  size_position_(0),
  payload_pos_(0),
  mode_(kFile),
  max_cluster_duration_(0),
  max_cluster_size_(0),
  last_timestamp_(0),
  output_cues_(true),
  cues_track_(0),
  frames_size_(0),
  frames_capacity_(0),
  frames_(NULL) {
  assert(writer_);

  // TODO: Create an Init function for Segment.
  segment_info_.Init();
}

Segment::~Segment() {
  if (cluster_list_) {
    for (int i=0; i<cluster_list_size_; ++i) {
      Cluster* const cluster = cluster_list_[i];
      delete cluster;
    }
    delete [] cluster_list_;
  }

  if (frames_) {
    for (int i=0; i<frames_size_; ++i) {
      Frame* const frame = frames_[i];
      delete frame;
    }
    delete [] frames_;
  }
}

bool Segment::Finalize() {
  if (!WriteFramesAll())
   return false;

  if (mode_ == kFile) {
    if (cluster_list_size_ > 0) {
      // Update last cluster's size
      Cluster* old_cluster = cluster_list_[cluster_list_size_-1];
      assert(old_cluster);

      if (!old_cluster->Finalize())
        return false;
    }

    const double duration =
      static_cast<double>(last_timestamp_) / segment_info_.timecode_scale();
    segment_info_.duration(duration);
    if (!segment_info_.Finalize(writer_))
      return false;

    // TODO: Add support for putting the Cues at the front.
    if (!seek_head_.AddSeekEntry(kMkvCues, writer_->Position() - payload_pos_))
      return false;

    if (!cues_.Write(writer_))
      return false;

    if (!seek_head_.Finalize(writer_))
      return false;

    if (writer_->Seekable()) {
      assert(size_position_ != -1);

      const int64 pos = writer_->Position();

      // -8 for the size of the segment size
      const int64 segment_size = pos - size_position_ - 8;
      assert(segment_size > 0);

      if (writer_->Position(size_position_))
        return false;

      if (WriteUIntSize(writer_, segment_size, 8))
        return false;

      if (writer_->Position(pos))
        return false;
    }
  }

  return true;
}

uint64 Segment::AddVideoTrack(int width, int height) {
  VideoTrack* vid_track = new (std::nothrow) VideoTrack();
  if (!vid_track)
    return 0;

  vid_track->type(Tracks::kVideo);
  vid_track->codec_id("V_VP8");
  vid_track->width(width);
  vid_track->height(height);

  m_tracks_.AddTrack(vid_track);
  has_video_ = true;

  return vid_track->number();
}

uint64 Segment::AddAudioTrack(int sample_rate, int channels) {
  AudioTrack* aud_track = new (std::nothrow) AudioTrack();
  if (!aud_track)
    return 0;

  aud_track->type(Tracks::kAudio);
  aud_track->codec_id("A_VORBIS");
  aud_track->sample_rate(sample_rate);
  aud_track->channels(channels);

  m_tracks_.AddTrack(aud_track);

  return aud_track->number();
}

bool Segment::AddFrame(uint8* frame,
                       uint64 length,
                       uint64 track_number,
                       uint64 timestamp,
                       bool is_key) {
  assert(frame);
  assert(length >= 0);
  assert(track_number >= 0);

  if (!header_written_) {
    if (!WriteSegmentHeader())
      return false;

    if (!seek_head_.AddSeekEntry(kMkvCluster,
                                 writer_->Position() - payload_pos_))
      return false;

    if (output_cues_ && cues_track_ == 0) {
      // Check for a video track
      for (int i=0; i<m_tracks_.GetTracksCount(); ++i) {
        const Track* pTrack = m_tracks_.GetTrackByIndex(i);
        assert(pTrack);

        if (m_tracks_.TrackIsVideo(pTrack->number())) {
          cues_track_ = pTrack->number();
          break;
        }
      }

      // Set first track found
      if (cues_track_ == 0) {
        const Track* pTrack = m_tracks_.GetTrackByIndex(0);
        assert(pTrack);
        cues_track_ = pTrack->number();
      }
    }
  }

  // If the segment has a video track hold onto audio frames to make sure the
  // audio that is associated with the start time of a video key-frame is
  // muxed into the same cluster.
  if (has_video_ && m_tracks_.TrackIsAudio(track_number)) {
    Frame* new_frame = new Frame();
    if (!new_frame->Init(frame, length))
      return false;
    new_frame->track_number(track_number);
    new_frame->timestamp(timestamp);
    new_frame->is_key(is_key);

    if (!QueueFrame(new_frame))
      return false;

    return true;
  }

  // Check to see if the muxer needs to start a new cluster.
  if (is_key && m_tracks_.TrackIsVideo(track_number)) {
    new_cluster_ = true;
  } else if (cluster_list_size_ > 0 ) {
    Cluster* cluster = cluster_list_[cluster_list_size_-1];
    assert(cluster);
    const uint64 cluster_ts =
      cluster->timecode() * segment_info_.timecode_scale();

    if (max_cluster_duration_ > 0 &&
      (timestamp - cluster_ts) >= max_cluster_duration_) {
        new_cluster_ = true;
    } else if (max_cluster_size_ > 0 && cluster_list_size_ > 0) {
      if (cluster->payload_size() >= max_cluster_size_) {
        new_cluster_ = true;
      }
    }
  }

  if (new_cluster_) {
    const int new_size = cluster_list_size_ + 1;

    if (new_size > cluster_list_capacity_) {
      // Add more clusters.
      const int new_capacity =
        (!cluster_list_capacity_)? 2 : cluster_list_capacity_*2;

      assert(new_capacity > 0);
      Cluster** clusters = new (std::nothrow) Cluster*[new_capacity];
      if (!clusters)
        return false;

      for (int i=0; i<cluster_list_size_; ++i) {
        clusters[i] = cluster_list_[i];
      }

      delete [] cluster_list_;

      cluster_list_ = clusters;
      cluster_list_capacity_ = new_capacity;
    }

    if (!WriteFramesLessThan(timestamp))
      return false;

    uint64 audio_timecode = 0;
    uint64 timecode = timestamp / segment_info_.timecode_scale();
    if (frames_size_ > 0) {
      audio_timecode =
        frames_[0]->timestamp() / segment_info_.timecode_scale();

      // Update the cluster's timecode to match the first audio frame.
      if (audio_timecode < timecode)
        timecode = audio_timecode;
    }
    
    // TODO: Add checks here to make sure the timestamps passed in are valid.

    cluster_list_[cluster_list_size_] = new (std::nothrow) Cluster(timecode,
                                                                   writer_);
    if (!cluster_list_[cluster_list_size_])
      return false;
    cluster_list_size_ = new_size;

    if (mode_ == kFile) { 
      if (cluster_list_size_ > 1) {
        // Update old cluster's size
        Cluster* old_cluster = cluster_list_[cluster_list_size_-2];
        assert(old_cluster);

        if (!old_cluster->Finalize())
          return false;
      }

      if (output_cues_)
        new_cuepoint_ = true;
    }

    new_cluster_ = false;
  }

  // Write any audio frames left.
  if (!WriteFramesAll())
   return false;

  assert(cluster_list_size_ > 0);
  Cluster* cluster = cluster_list_[cluster_list_size_-1];
  assert(cluster);

  int64 block_timecode = timestamp / segment_info_.timecode_scale();
  block_timecode -= static_cast<int64>(cluster->timecode());
  assert(block_timecode >= 0);

  if (new_cuepoint_ && cues_track_ == track_number) {
    if (!AddCuePoint(timestamp))
      return false;
  }

  if (!cluster->AddFrame(frame,
                         length,
                         track_number,
                         static_cast<short>(block_timecode),
                         is_key))
    return false;

  if (timestamp > last_timestamp_)
    last_timestamp_ = timestamp;

  return true;
}

void Segment::OutputCues(bool output_cues) {
  output_cues_ = output_cues;
}

bool Segment::CuesTrack(uint64 track) {
  Track* pTrack = GetTrackByNumber(track);
  if (!pTrack)
    return false;

  cues_track_ = track;
  return true;
}

Track* Segment::GetTrackByNumber(uint64 track_number) {
  return m_tracks_.GetTrackByNumber(track_number);
}

bool Segment::WriteSegmentHeader() {
  // Write "unknown" (-1) as segment size value. If mode is kFile, Segment
  // will write over duration when the file is finalized.
  if (SerializeInt(writer_, kMkvSegment, 4))
    return false;

  // Save for later.
  size_position_ = writer_->Position();

  // We need to write 8 bytes because if we are going to overwrite the segment
  // size later we do not know how big our segment will be.
  if (SerializeInt(writer_, 0x01FFFFFFFFFFFFFFULL, 8))
    return false;

  payload_pos_ =  writer_->Position();

  if (mode_ == kFile && writer_->Seekable()) {
    // Set the duration > 0.0 so SegmentInfo will write out the duration. When
    // the muxer is done writing we will set the correct duration and have
    // SegmentInfo upadte it.
    segment_info_.duration(1.0);

    if (!seek_head_.Write(writer_))
      return false;
  }

  if (!seek_head_.AddSeekEntry(kMkvInfo, writer_->Position() - payload_pos_))
    return false;
  if (!segment_info_.Write(writer_))
    return false;

  if (!seek_head_.AddSeekEntry(kMkvTracks, writer_->Position() - payload_pos_))
    return false;
  if (!m_tracks_.Write(writer_))
    return false;
  header_written_ = true;

  return true;
}

bool Segment::AddCuePoint(uint64 timestamp) {
  assert(cluster_list_size_ > 0);
  Cluster* cluster = cluster_list_[cluster_list_size_-1];
  assert(cluster);

  CuePoint* cue = new (std::nothrow) CuePoint();
  if (!cue)
    return false;

  cue->time(timestamp / segment_info_.timecode_scale());
  cue->block_number(cluster->blocks_added() + 1);
  cue->cluster_pos(writer_->Position() - payload_pos_);
  cue->track(cues_track_);
  if (!cues_.AddCue(cue))
    return false;

  new_cuepoint_ = false;
  return true;
}

bool Segment::QueueFrame(Frame* frame) {
  const int new_size = frames_size_ + 1;

  if (new_size > frames_capacity_) {
    // Add more frames.
    const int new_capacity = (!frames_capacity_)? 2 : frames_capacity_*2;
    assert(new_capacity > 0);

    Frame** frames = new (std::nothrow) Frame*[new_capacity];
    if (!frames)
      return false;

    for (int i=0; i<frames_size_; ++i) {
      frames[i] = frames_[i];
    }

    delete [] frames_;
    frames_ = frames;
    frames_capacity_ = new_capacity;
  }

  frames_[frames_size_++] = frame;

  return true;
}

bool Segment::WriteFramesAll() {
  if (frames_) {
    assert(cluster_list_size_ > 0);
    Cluster* cluster = cluster_list_[cluster_list_size_-1];
    assert(cluster);

    for (int i=0; i<frames_size_; ++i) {
      Frame* const frame = frames_[i];

      int64 block_timecode =
        frame->timestamp() / segment_info_.timecode_scale();
      block_timecode -= static_cast<int64>(cluster->timecode());
      assert(block_timecode >= 0);

      if (new_cuepoint_ && cues_track_ == frame->track_number()) {
        if (!AddCuePoint(frame->timestamp()))
          return false;
      }

      if (!cluster->AddFrame(frame->frame(),
                             frame->length(),
                             frame->track_number(),
                             static_cast<short>(block_timecode),
                             frame->is_key()))
        return false;

      if (frame->timestamp() > last_timestamp_)
        last_timestamp_ = frame->timestamp();

      delete frame;
    }

    frames_size_ = 0;
  }

  return true;
}

bool Segment::WriteFramesLessThan(uint64 timestamp) {
  if (frames_size_ > 0) {
    assert(frames_);
    assert(cluster_list_size_ > 0);
    Cluster* cluster = cluster_list_[cluster_list_size_-1];
    assert(cluster);

    int shift_left = 0;

    // TODO: Change this to use the durations of frames instead of the next
    // frame's start time if the duration is accurate.
    for (int i=1; i<frames_size_; ++i) {
      const Frame* const frame_curr = frames_[i];

      if (frame_curr->timestamp() > timestamp)
        break;

      const Frame* const frame_prev = frames_[i-1];
      
      int64 block_timecode = frame_prev->timestamp() / segment_info_.timecode_scale();
      block_timecode -= static_cast<int64>(cluster->timecode());
      assert(block_timecode >= 0);

      if (new_cuepoint_ && cues_track_ == frame_prev->track_number()) {
        if (!AddCuePoint(frame_prev->timestamp()))
          return false;
      }

      if (!cluster->AddFrame(frame_prev->frame(),
                             frame_prev->length(),
                             frame_prev->track_number(),
                             static_cast<short>(block_timecode),
                             frame_prev->is_key()))
        return false;

      ++shift_left;
      if (frame_prev->timestamp() > last_timestamp_)
        last_timestamp_ = frame_prev->timestamp();

      delete frame_prev;
    }

    if (shift_left > 0) {
      assert(shift_left < frames_size_);

      const int new_frames_size = frames_size_ - shift_left;
      for (int i=0; i<new_frames_size; ++i) {
        frames_[i] = frames_[i+shift_left];
      }

      frames_size_ = new_frames_size;
    }
  }

  return true;
}

}  // namespace mkvmuxer
