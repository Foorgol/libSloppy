/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SLOPPY__NET_H
#define SLOPPY__NET_H

#include <string>
#include <vector>

#ifndef WIN32
#include <netdb.h>
#endif

#ifdef WIN32
#include <winsock2.h>   // for htonl() etc.
#endif

#include "../String.h"
#include "../Memory.h"

using namespace std;

namespace Sloppy
{
  namespace Net
  {
    class InvalidHostname{};

    using ByteString = basic_string<uint8_t>;

#ifndef WIN32
    sockaddr_in fillSockAddr(const string& hostName, int port);
#endif

    //----------------------------------------------------------------------------

    uint64_t hton_sizet(const uint64_t& in);
    uint64_t ntoh_sizet(const uint64_t& in);

    //----------------------------------------------------------------------------


    /** \brief A class for constructing a binary blob of data that consists of a sequence of simple data types (int, longs, ...)
     *
     * The class OWNS and copies all data that is passed to it during message construction.
     *
     * \note Uses default, compiler-generated ctor, dtor and copy/move assignment because internally
     * it only wraps a basic string.
     */
    class OutMessage
    {
    public:
      /** \brief Appends a string to the message
       */
      void addString(
          const string& s      ///< the string to append to the message
          );

      /** \brief Appends a single byte to the message
       */
      void addByte(
          uint8_t b      ///< the byte to append to the message
          );

      /** \brief Appends an integer value to the message
       */
      void addInt(
          int i      ///< the integer to append to the message
          );

      /** \brief Appends an unsigned short (unsigned 16-bit value) to the message
       */
      void addUI16(
          uint16_t u      ///< the value to append to the message
          );

      /** \brief Appends an unsigned integer (unsigned 32-bit value) to the message
       */
      void addUI32(
          uint32_t u      ///< the value to append to the message
          );

      /** \brief Appends an unsigned long (unsigned 64-bit value) to the message
       */
      void addUI64(
          uint64_t u      ///< the value to append to the message
          );

      /** \brief Appends boolean value to the message
       *
       * The boolean value is internally converted to a single byte of either `1` or `0`
       */
      void addBool(
          bool b      ///< the value to append to the message
          )
      { addByte(b ? 1 : 0); }

      /** \brief Appends the contents of a `MemView` to the message
       *
       * The data is copied from the view to the message.
       *
       * \throws std::length_error if appending the data would exceed the maximum string size
       * \throws std::bad_alloc if the necessary memory could not be allocated
       */
      void addMemView(const ArrayView<uint8_t>& mv     ///< the view with the data to be added
          );

      /** \brief Appends a byte string to the message
       */
      void addByteString(
          const ByteString& bs    ///< the byte string to add
          );

      /** \brief adds a list of other OutMessages to the message
       */
      void addMessageList(
          const vector<OutMessage>& msgList
          );

      /** \brief Allows for direct, low-level manipulation of the message data
       *
       * This can destroy the message structure. Use only if you know what
       * you're doing!
       *
       * \throws std::out_of_range if the write would exceed the current message limits; the whole message will remain untouched if this exception occurs
       */
      void rawPoke(
          const MemView& mv,     ///< the view with the data to be written
          size_t dstOffset       ///< the index of the first message byte that shall be written
          );

      /** \returns a read-only reference to the current message data
       */
      const ByteString& getDataAsRef() const { return data; }

      /** \returns a copy of the current message data
       */
      ByteString getDataAsCopy() const { return data; }

      /** \returns the current size of the message in bytes
       */
      size_t getSize() const { return data.size(); }

      /** \brief Gives view access to the message data
       *
       * \note It is the caller's responsibility that the underlying message data remains
       * valid as long as the view object is used!
       *
       * \returns an ArrayView object that points to the message data
       */
      MemView view();

      void clear() { data.clear(); }

    private:
      ByteString data{};
    };

    //----------------------------------------------------------------------------

    /** \brief A class that consumes data blobs generated by `OutMessage` and sequentially dissects this data
     *
     * \note In order to avoid copies, this class DOES NOT OWN the data it works on. The user
     * has to ensure that the source data object (e.g., a string) lives at least as long
     * as the associated instance of this class is used. The only exception is if the
     * instance has been created with the static factory function `fromDataCopy()`.
     */
    class InMessage
    {
    public:
      /** \brief Default ctor that initializes the class with an empty mem view
       * that makes the new instance basically unusable.
       */
      InMessage() = default;

      /** \brief Ctor from a view on a memory array
       *
       * The new instance does NOT take ownership of the data and does not copy it.
       * The caller has to ensure that the referenced location exists long enough.
       */
      explicit InMessage(
          const MemView& v   ///< a view on the data that shall be dissected
          )
        :fullView{v}, curView{v} {}

      /** \brief Ctor from a string
       *
       * The new instance does NOT take ownership of the data and does not copy it.
       * The caller has to ensure that the referenced string exists long enough.
       */
      explicit InMessage(
          const string& s   ///< a string with the data that shall be dissected
          )
        : InMessage(MemView{s}) {}

      /** \brief Ctor from a ByteString
       *
       * The new instance does NOT take ownership of the data and does not copy it.
       * The caller has to ensure that the referenced string exists long enough.
       */
      explicit InMessage(
          const ByteString& bs   ///< a ByteString with the data that shall be dissected
          )
        : InMessage(MemView(bs.c_str(), bs.size())) {}

      /** \brief Copy ctor
       *
       * Creates a deep copy of the data if we own it. If we don't own it,
       * we simply copy the data views of the source object over to the new object
       */
      InMessage(const InMessage& other);

      /** \brief Copy assignment
       *
       * Creates a deep copy of the data if we own it. If we don't own it,
       * we simply copy the data views of the source object over to the new object
       */
      InMessage& operator =(const InMessage& other);

      /** \brief Move assignment
       *
       * Shifts all pointers and data over to the new owner and leaves the
       * source object in the same state as the default ctor
       */
      InMessage& operator =(InMessage&& other) noexcept;

      /** \brief Move ctor
       *
       * Shifts all pointers and data over to the new owner and leaves the
       * source object in the same state as the default ctor
       */
      InMessage(InMessage&& other) noexcept;


      /** \brief Factory function for creating a new InMessage that actually owns the data it processes
       *
       * \throws std::bad_alloc if problems during memory allocation occured
       */
      static InMessage fromDataCopy(
          const MemView& v   ///< a view on the data that shall be copied and dissected
          );

      /** \brief Factory function for creating a new InMessage that actually owns the data it processes
       *
       * \throws std::bad_alloc if problems during memory allocation occured
       */
      static InMessage fromDataCopy(
          const ByteString& bs   ///< a ByteString with the data that shall be copied and dissected
          )
      {
        return fromDataCopy(MemView(bs.c_str(), bs.size()));
      }

      /** \brief Interprets the next bytes of the message as a string and extracts it from the message.
       *
       * Fast-forwards the read position in the message when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns an estring containing the string data
       */
      estring getString();

      /** \brief Interprets the next bytes of the message as a string and extracts it from the message.
       *
       * Fast-forwards the read position in the message when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns an estring containing the string data
       */
      ByteString getByteString();

      /** \brief Gets the next byte from the message.
       *
       * Fast-forwards the read position in the message when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns the read byte
       */
      uint8_t getByte();

      /** \brief Interprets the next bytes of the message as an integer and extracts it from the message.
       *
       * Fast-forwards the read position in the message when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns an integer containing the read data
       */
      int getInt();

      /** \brief Interprets the next bytes of the message as an UINT16 and extracts it from the message.
       *
       * Fast-forwards the read position in the message when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns an UINT16 containing the read data
       */
      uint16_t getUI16();

      /** \brief Interprets the next bytes of the message as an UINT32 and extracts it from the message.
       *
       * Fast-forwards the read position in the message when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns an UINT32 containing the read data
       */
      uint32_t getUI32();

      /** \brief Interprets the next bytes of the message as an UINT32 and extracts it from the message.
       *
       * *DOES NOT fast-forward the read position in the message* when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns an UINT32 containing the read data
       */
      uint32_t peekUI32();

      /** \brief Interprets the next bytes of the message as an UINT64 and extracts it from the message.
       *
       * Fast-forwards the read position in the message when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns an UINT64 containing the read data
       */
      uint64_t getUI64();

      /** \brief Interprets the next bytes of the message as an UINT64 and extracts it from the message.
       *
       * *DOES NOT fast-forward the read position in the message* when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns an UINT64 containing the read data
       */
      uint64_t peekUI64();

      /** \brief Interprets the next byte of the message as a bool and extracts it from the message.
       *
       * Fast-forwards the read position in the message when the data has been read.
       *
       * \throws std::out_of_range if the message does not contain any more bytes.
       *
       * \returns a bool representing the read byte
       */
      bool getBool();

      /** \brief Extracts and returns a list of (sub-)messages.
       *
       * The list of sub-messages shall be proceeded by a UI64 value containing the
       * number of elements in the list.
       *
       * Each submessage is in itself handled like a byte string (UI64 length header
       * followed by the actual data).
       *
       * The returned sub-messages OWN their data. This is because it is likely that
       * the submessages live longer than their frame message.
       */
      vector<InMessage> getMessageList();

      /** \brief Extracts a data block as a plain memory buffer.
       *
       * The memory block has to be preceeded by a UI64 length tag.
       *
       * The returned data is a DEEP COPY of the source data.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns a heap-allocated buffer containing a copy the next `n` bytes
       * of the source data where `n` is derived from a UI64 length tag at the
       * current read position.
       */
      MemArray getMemArray();

      /** \brief Extracts a data block as a plain memory buffer.
       *
       * The memory block has to be preceeded by a UI64 length tag.
       *
       * The returned data is a VIEW of the source data that becomes invalid
       * when the source data block becomes invalid.
       *
       * The purpose of this call is to pass the resulting view to e.g., other
       * Ctors that create a copy of the view themselves. This saves one
       * copy operation compared to calling `getMemArray()`.
       *
       * \throws std::out_of_range if the message does not contain sufficient bytes for the read operation.
       *
       * \returns a view on the next `n` bytes
       * of the source data where `n` is derived from a UI64 length tag at the
       * current read position.
       */
      MemView getMemView();

    private:
      MemView fullView{};   ///< contains the full data view as passed to the ctor
      MemView curView{};    ///< contains a partial view that always starts at the next read position
      MemArray data{};      ///< potential internal data buffer; used only if created via `fromDataCopy`

      /** \brief Checks if the message still contains a certain number of bytes.
       *
       * \throws std::out_of_range if the message does not contain sufficient data.
       */
      void assertSufficientData(size_t n) const;
    };

    //----------------------------------------------------------------------------

    /** \brief A specialized OutMessage that starts with a header containing an integer representation of an enum
     *
     * This class shall facilitate the construction of messages that start with
     * a message type header defined as an enum.
     */
    template<typename TypeEnum>
    class TypedOutMessage : public OutMessage
    {
    public:

      /** \brief Deleted default ctor; we always need a message type for construction
       */
      TypedOutMessage() = delete;

      /** \brief Constructor of an otherwise empty message, except for the starting type header
       */
      TypedOutMessage(
          const TypeEnum& _msgType   ///< the type of the message to construct
          )
        :OutMessage{}, msgType{_msgType}
      {
        addUI32(static_cast<uint32_t>(msgType));
      }

      /** \brief Copy ctor; copies all contained data
       */
      TypedOutMessage(const TypedOutMessage& other) = default;

      /** \brief Copy assignment; copies all contained data
       */
      TypedOutMessage& operator=(const TypedOutMessage& other) = default;

      /** \brief Move ctor; moves the containted data and copies the type
       */
      TypedOutMessage(TypedOutMessage&& other) noexcept
        :OutMessage{std::move(other)}
      {
        msgType = other.msgType;
      }

      /** \brief Move assignment; moves the containted data and copies the type
       */
      TypedOutMessage& operator=(TypedOutMessage&& other) noexcept
      {
        OutMessage::operator=(std::move(other));
        msgType = other.msgType;
      }

      /** \brief Overwrites the message header with a new type
       */
      void rewriteType(
          const TypeEnum& newType  ///< the new type to assign to the message
          )
      {
        uint32_t networkOrder = htonl(static_cast<uint32_t>(newType));
        MemView mv{reinterpret_cast<const uint8_t*>(&networkOrder), 4};
        rawPoke(mv, 0);
        msgType = newType;
      }

    private:
      TypeEnum msgType;
    };

    //----------------------------------------------------------------------------

    /** \brief A specialized InMessage that assumes that the message starts with a header
     * containing an integer representation of an enum
     *
     * This class shall facilitate the dissection of messages that start with
     * a message type header defined as an enum.
     *
     * Just like its base class, InMessage, this class DOES NOT OWN OR COPY
     * the data that is passed to it during construction. It is the caller's
     * responsibility that the source data object lives long enough.
     *
     * \note This generic class cannot check whether the type header contains a valid
     * value or not. If the header value does not match to any enum value, the derived
     * message type is undefined.
     *
     */
    template<typename TypeEnum>
    class TypedInMessage : public InMessage
    {
    public:
      /** \brief Deleted default ctor; we always need a message for construction in
       * order to properly derive the message type
       */
      TypedInMessage() = delete;

      /** \brief Ctor from a standard string
       *
       * \throws std::out_of_range if the source data is not sufficiently long for parsing the message type
       */
      TypedInMessage(
          const string& s   ///< the string containing the source data
          )
        :InMessage{s}, msgType{static_cast<TypeEnum>(getUI32())} {}

      /** \brief Ctor from a byte string
       *
       * \throws std::out_of_range if the source data is not sufficiently long for parsing the message type
       */
      TypedInMessage(
          const ByteString& bs   ///< the string containing the source data
          )
        :InMessage{bs}, msgType{static_cast<TypeEnum>(getUI32())} {}

      /** \brief Ctor from a memory view
       *
       * The content of the view is not copied! The memory has
       * to remain available as long as this instance is used!
       *
       * \throws std::out_of_range if the source data is not sufficiently long for parsing the message type
       */
      TypedInMessage(
          const MemView& mv   ///< a view of the memory with the source data for dissection
          )
        :InMessage{mv}, msgType{static_cast<TypeEnum>(getUI32())} {}

      /** \brief Copy ctor; copies all contained data
       */
      TypedInMessage(const TypedInMessage& other) = default;

      /** \brief Copy assignment; copies all contained data
       */
      TypedInMessage& operator=(const TypedInMessage& other) = default;

      /** \brief Move ctor; moves the containted data and copies the type
       */
      TypedInMessage(TypedInMessage&& other) noexcept
        :InMessage{std::move(other)}
      {
        msgType = other.msgType;
      }

      /** \brief Move assignment; moves the containted data and copies the type
       */
      TypedInMessage& operator=(TypedInMessage&& other) noexcept
      {
        InMessage::operator=(std::move(other));
        msgType = other.msgType;

        return *this;
      }

      /** \returns the message type
       *
       * \note This generic class cannot check whether the type header contains a valid
       * value or not. If the header value does not match to any enum value, the derived
       * message type is undefined.
       *
       */
      TypeEnum getType() const { return msgType; }

    private:
      TypeEnum msgType;
    };
  }
}

#endif
