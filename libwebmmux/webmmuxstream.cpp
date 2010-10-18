// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cstdlib>
#include <cassert>
#include <climits>
#include "webmmuxstream.hpp"
#include "webmmuxcontext.hpp"


namespace webmmux
{

Stream::Frame::Frame()
{
}


Stream::Frame::~Frame()
{
}


void Stream::Frame::Release()
{
   delete this;
}



Stream::Stream(Context& c) :
    m_context(c),
    m_trackNumber(0)
{
}


Stream::~Stream()
{
}


void Stream::Final()
{
}


void Stream::SetTrackNumber(int tn)
{
   m_trackNumber = tn;
}


int Stream::GetTrackNumber() const
{
   return m_trackNumber;
}


void Stream::WriteTrackEntry(int tn)
{
    EbmlIO::File& f = m_context.m_file;

    f.WriteID1(0xAE);  //TrackEntry ID (level 2)

    //allocate 2 bytes for track entry size field
    const long long begin_pos = f.SetPosition(2, std::ios_base::cur);

    WriteTrackNumber(tn);
    WriteTrackUID();
    WriteTrackType();
    WriteTrackName();
    WriteTrackCodecID();
    WriteTrackCodecPrivate();
    WriteTrackCodecName();
    WriteTrackSettings();

    const long long end_pos = f.GetPosition();

    const long long size_ = end_pos - begin_pos;
    assert(size_ <= USHRT_MAX);

    const unsigned short size = static_cast<unsigned short>(size_);

    f.SetPosition(begin_pos - 2);
    f.Write2UInt(size);

    f.SetPosition(end_pos);
}


void Stream::WriteTrackNumber(int tn_)
{
    EbmlIO::File& f = m_context.m_file;

    assert(tn_ > 0);
    assert(tn_ < 128);

    m_trackNumber = tn_;

    const unsigned char tn = static_cast<unsigned char>(tn_);

    f.WriteID1(0xD7);     //track number ID  //1101 0111
    f.Write1UInt(1);
    f.Serialize1UInt(tn);
}


void Stream::WriteTrackUID()
{
    EbmlIO::File& f = m_context.m_file;

    const TrackUID_t uid = CreateTrackUID();

    f.WriteID2(0x73C5);    //TrackUID ID
    f.Write1UInt(8);
    f.Serialize8UInt(uid);
}


void Stream::WriteTrackName()
{
}


void Stream::WriteTrackCodecPrivate()
{
}


void Stream::WriteTrackSettings()
{
}


Stream::TrackUID_t Stream::CreateTrackUID()
{
    TrackUID_t result;

    //TODO: Do we need to do this?
    //NOTE: The TrackUID is serialized in the normal way (the same
    //as for any other integer that is the payload of an EBML tag),
    //but the Matroska spec does say that this is an unsigned
    //integer.  In order to allow this integer value to be used
    //as an EBML varying-size integer, we restrict its value so
    //that it satifies the constraints for a varying size integer
    //that is streamed out using 8 bytes.  That means the upper
    //byte (the first in the stream) is 0 (the upper byte in the
    //stream is reserved for indicating that this integer
    //occupies 8 bytes total in the stream), and the low order
    //byte (the last in the stream) is even, which prevents
    //the integer from ever having a value with all bits set
    //(because then it would look like a signed integer).

    unsigned char* p = reinterpret_cast<unsigned char*>(&result);
    unsigned char* const q = p + 7;

    {
        const int n = rand();

        unsigned char& b0 = *p++;

        b0 = static_cast<unsigned char>(n >> 4); //throw away low-order bits

        b0 &= 0xFE;  //ensure low order bit is not set
    }

    while (p != q)
    {
        const int n = rand();
        *p++ = static_cast<unsigned char>(n >> 4); //throw away low-order bits
    }

    *p = 0;

    return result;
}


void Stream::Frame::Write(
    const Stream& s,
    unsigned long cluster_timecode) const
{
    EbmlIO::File& file = s.m_context.m_file;

    const unsigned long block_size = 1 + 2 + 1 + GetSize();     //tn, tc, flg, f

    //begin block

    file.WriteID1(0xA3);    //SimpleBlock ID
    file.Write4UInt(block_size);

#ifdef _DEBUG
    const long long pos = file.GetPosition();
#endif

    const int tn_ = s.GetTrackNumber();
    assert(tn_ > 0);
    assert(tn_ <= 255);

    const unsigned char tn = static_cast<unsigned char>(tn_);

    file.Write1UInt(tn);   //track number

    const unsigned long ft = GetTimecode();

    {
        const long tc_ = long(ft) - long(cluster_timecode);
        assert(tc_ >= SHRT_MIN);
        assert(tc_ <= SHRT_MAX);

        const short tc = static_cast<short>(tc_);

        file.Serialize2SInt(tc);       //relative timecode
    }

    unsigned char flags = 0;

    if (IsKey())
        flags |= (unsigned char)(1 << 7);

    file.Write(&flags, 1);   //written as binary, not uint

    file.Write(GetData(), GetSize());  //frame

    //end block

#ifdef _DEBUG
    const long long newpos = file.GetPosition();
    assert((newpos - pos) == block_size);
#endif

}



}  //end namespace webmmux
