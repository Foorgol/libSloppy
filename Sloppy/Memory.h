/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
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

#ifndef __LIBSLOPPY_MEMORY_H
#define __LIBSLOPPY_MEMORY_H

#include <memory>
#include <stdexcept>
#include <string.h>    // for memcpy

using namespace std;

namespace Sloppy
{
  /** \brief A read-only class for arrays of any type
   *
   * \note Instances of this class DO NOT OWN the pointers / memory they are working with.
   */
  template <class T>
  class ArrayView
  {
  public:
    /** \brief Default ctor for an empty, invalid array
     */
    ArrayView() = default;  // uses member initializiation defaults

    /** \brief Ctor for an existing array with a defined number of elements
     *
     * \throws std::invalid_argument if the caller uses either a valid pointer with zero elements or non-zero elements with a null pointer
     */
    ArrayView(
        const T* startPtr,   ///< pointer to the first element of the array
        size_t nElem         ///< number of elements in the array (NOT number of bytes!!)
        )
      : ptr{startPtr}, cnt{nElem}
    {
      if ((startPtr == nullptr) && (nElem != 0))
      {
        throw std::invalid_argument("Array view: inconsistent ctor parameters");
      }
      if ((startPtr != nullptr) && (nElem == 0))
      {
        throw std::invalid_argument("Array view: inconsistent ctor parameters");
      }
    }

    /** \brief Copy constructor
     *
     * This ctor DOES NOT create a deep copy of the array; it simply
     * copies the pointer and the array size.
     */
    explicit ArrayView(
        const ArrayView<T>& other   ///< the view to copy
        ) noexcept
    {
      ptr = other.ptr;
      cnt = other.cnt;
    }

    /** \brief Copy Assignment
     *
     * This operator DOES NOT create a deep copy of the array; it simply
     * copies the pointer and the array size.
     */
    ArrayView<T>& operator=(const ArrayView<T>& other) noexcept
    {
      ptr = other.ptr;
      cnt = other.cnt;
      return *this;
    }

    /** \brief Move constructor
     *
     * Takes over the pointer from the source and sets the source
     * pointer to zero.
     */
    ArrayView(ArrayView<T>&& other) noexcept
    {
      ptr = other.ptr;
      cnt = other.cnt;
      other.ptr = nullptr;
      other.cnt = 0;
    }

    /** \brief Move assignment.
     *
     * Takes over the pointer from the source and sets the source
     * pointer to zero.
     */
    ArrayView<T>& operator =(ArrayView<T>&& other)
    {
      // check for self-assignment
      if (this == &other) return *this;

      ptr = other.ptr;
      cnt = other.cnt;
      other.ptr = nullptr;
      other.cnt = 0;
    }

    /** \brief Default dtor, no ressources to release
     */
    virtual ~ArrayView() = default;

    /** \brief Provides read access to an element in the array
     *
     * \throws std::out_of_range if the index is outside the array's bounds
     */
    const T& elemAt(size_t idx) const
    {
      if (idx >= cnt)
      {
        throw std::out_of_range("Array view: access beyond array bounds!");
      }
      return *(ptr + idx);
    }

    /** \returns the number of elements in the array
     *
     * \note The return value is usually NOT IDENTICAL with the number of BYTES allocated by the array!
     */
    size_t size() const
    {
      return cnt;
    }

    /** \returns the number of bytes in the array
     *
     * \note The return value is usually NOT IDENTICAL with the number of ELEMENTS the array!
     */
    size_t byteSize() const
    {
      return cnt * sizeof(T);
    }

    /** \returns `true` if the array contains elements, `false` otherwise
     */
    bool notEmpty() const
    {
      return (cnt > 0);
    }

    /** \returns `true` if the array is empty, `false` otherwise
     */
    bool empty() const
    {
      return (cnt == 0);
    }

    /** \returns A new ArrayView that only contains a subset of this view
     *
     * \throws std::out_of_range if one or more parameters are out of range
     * \throws std::invalid_argument if the last index is before the first index
     */
    ArrayView<T> slice_byIdx(
        size_t idxFirst,    ///< index of the first element in the new view
        size_t idxLast      ///< index of the last element in the new view
        ) const
    {
      if (idxLast < idxFirst)
      {
        throw std::invalid_argument("ArrayView: invalid indices for slicing");
      }
      if ((idxFirst >= cnt) || (idxLast >= cnt))
      {
        throw std::out_of_range("ArrayView: indices out of bounds for slicing");
      }

      return ArrayView<T>(ptr + idxFirst, idxLast - idxFirst + 1);
    }

    /** \returns A new ArrayView that only contains a subset of this view
     *
     * \throws std::out_of_range if one or more parameters are out of range
     */
    ArrayView<T> slice_byCount(
        size_t idxFirst,    ///< index of the first element in the new view
        size_t n            ///< number of elements in the subset
        ) const
    {
      if (idxFirst >= cnt)
      {
        throw std::out_of_range("ArrayView: index out of bounds for slicing");
      }

      if (n == 0) return ArrayView<T>{};

      if ((idxFirst + n - 1) >= cnt)
      {
        throw std::out_of_range("ArrayView: element count out of bounds for slicing");
      }

      return ArrayView<T>(ptr + idxFirst, n);
    }

    /** \brief Chops of the first `n` characters on the left
     *
     * The view is modified in place.
     *
     * \throws std::out_of_range if the view contains less than 'n' elements
     */
    void chopLeft(size_t n)
    {
      if (n == 0) return;

      if (n > cnt)
      {
        throw std::out_of_range("ArrayView: too many elements to chop on the left");
      }
      if (n == cnt)
      {
        ptr = nullptr;
        cnt = 0;
      } else {
        ptr += n;
        cnt -= n;
      }
    }

    /** \brief Chops of the last `n` characters on the right
     *
     * The view is modified in place.
     *
     * \throws std::out_of_range if the view contains less than 'n' elements
     */
    void chopRight(size_t n)
    {
      if (n == 0) return;

      if (n > cnt)
      {
        throw std::out_of_range("ArrayView: too many elements to chop on the left");
      }
      if (n == cnt)
      {
        ptr = nullptr;
        cnt = 0;
      } else {
        cnt -= n;
      }
    }

    /** \returns a reference to the first element in the array
     *
     * \throws std::out_of_range if the array is empty
     */
    const T& first() const
    {
      if ((cnt == 0) || (ptr == nullptr))
      {
        throw std::out_of_range("ArrayView: attempt to access the first element of an empty array");
      }

      return *ptr;
    }

    /** \returns a reference to the last element in the array
     *
     * \throws std::out_of_range if the array is empty
     */
    const T& last() const
    {
      if ((cnt == 0) || (ptr == nullptr))
      {
        throw std::out_of_range("ArrayView: attempt to access the last element of an empty array");
      }

      return *(ptr + cnt - 1);
    }

    /** \returns a reference to the last element in the array
     *
     * \throws std::out_of_range if the array is empty
     */
    const T* lastPtr() const
    {
      if ((cnt == 0) || (ptr == nullptr))
      {
        throw std::out_of_range("ArrayView: attempt to access the last element of an empty array");
      }

      return (ptr + cnt - 1);
    }

    /** \brief Provides read access to an element in the array
     *
     * \throws std::out_of_range if the index is outside the array's bounds
     */
    const T& operator[](size_t idx) const
    {
      return elemAt(idx);
    }

    /** \returns the array's base pointer casted to `char*`
     */
    const char* to_charPtr() const
    {
      return reinterpret_cast<const char *>(ptr);
    }

    /** \returns the array's base pointer casted to `void*`
     */
    const void* to_voidPtr() const
    {
      return reinterpret_cast<const void *>(ptr);
    }

    /** \returns the array's base pointer casted to `uint8*`
     */
    const uint8_t* to_uint8Ptr() const
    {
      return reinterpret_cast<const uint8_t *>(ptr);
    }

    /** \returns the array's base pointer casted to `unsigned char*`
     */
    const unsigned char* to_ucPtr() const
    {
      return reinterpret_cast<const unsigned char *>(ptr);
    }

    /** \brief Comparison between ArrayViews
     *
     * \returns `true` if the base pointer and the size are equal; `false` otherwise
     */
    bool operator ==(const ArrayView<T>& other)
    {
      return ((ptr == other.ptr) && (cnt == other.cnt));
    }

    /** \brief Comparison between arrays, inverted
     *
     * \returns `true` if the base pointer or the size differ; `false` otherwise
     */
    bool operator !=(const ArrayView<T>& other)
    {
      return ((ptr != other.ptr) || (cnt != other.cnt));
    }

    /** \brief Comparison between array lengths
     *
     * \note This operator DOES NOT compare the base pointers, it simply compares
     * the array sizes! Thus, it can compare two completely different arrays that
     * have nothing in common!
     *
     * \returns `true` if the other array's size is larger than this array's size
     */
    bool operator >(const ArrayView<T>& other)
    {
      return cnt > other.cnt;
    }

    /** \brief Comparison between array lengths
     *
     * \note This operator DOES NOT compare the base pointers, it simply compares
     * the array sizes! Thus, it can compare two completely different arrays that
     * have nothing in common!
     *
     * \returns `true` if the other array's size is larger than this array's size
     */
    bool operator <(const ArrayView<T>& other)
    {
      return cnt < other.cnt;
    }

    /** \brief Converts the view into a "standardized" `uint8`-view ("ByteView")
     *
     * \returns an `ArrayView<uint8>` that covers the full array
     */
    ArrayView<uint8_t> toByteArrayView() const
    {
      return ArrayView<uint8_t>(reinterpret_cast<const uint8_t*>(ptr), cnt * sizeof(T));
    }

  private:
    const T* ptr{nullptr};
    size_t cnt{0};
  };

  //----------------------------------------------------------------------------

  /** \brief A specialized ArrayView for memory segments; uses UI8 (==> bytes) as internal format
   *
   * Offers the additional benefit of being constructable from char-pointers
   * (which is a common type when dealing with legacy C-functions) or `std::string`s.
   */
  class MemView : public ArrayView<uint8_t>
  {
  public:
    // inherit ArrayView's constructors
    using ArrayView<uint8_t>::ArrayView;

    /** \brief Ctor from a char-pointer that is being reinterpreted as a byte-pointer
     */
    MemView(const char* _ptr, size_t len)
      :ArrayView(reinterpret_cast<const uint8_t*>(_ptr), len) {}

    /** \brief Ctor from a standard string
     */
    explicit MemView(const string& src)
      :ArrayView(reinterpret_cast<const uint8_t*>(src.c_str()), src.size()) {}

    /** \brief Copy ctor
     *
     * Delegates the call to the parent's copy ctor.
     */
    MemView (const MemView& other)
      :ArrayView<uint8_t>(other) {}

    /** \brief Copy assignment
     *
     * Delegates the call to the parent's copy assignment operator.
     */
    MemView& operator=(const MemView& other)
    {
      return reinterpret_cast<MemView&>(ArrayView<uint8_t>::operator =(other));
    }

    /** \brief Move ctor
     *
     * Delegates the call to the parent's move ctor.
     */
    MemView (const MemView&& other)
      :ArrayView<uint8_t>(std::move(other)) {}

    /** \brief move assignment
     *
     * Delegates the call to the parent's move assignment operator.
     */
    MemView& operator=(const MemView&& other)
    {
      return reinterpret_cast<MemView&>(ArrayView<uint8_t>::operator =(std::move(other)));
    }

  };

  //----------------------------------------------------------------------------

  /** \brief A class for managed, heap-allocated arrays of any type;
   * derived classes can define their own functions for allocating or releasing memory.
   *
   * Can be used with owning as well as with non-owning pointers.
   */
  template <class T>
  class ManagedArray
  {
  public:
    /** \brief Default ctor that creates an empty, invalid array
     */
    ManagedArray() = default;

    /** \brief Ctor that allocates a new array with a defined number of elements
     *
     * Can throw any exception that the custom `allocateMem()` throws.
     *
     * \throws std::runtime_error if the custom 'allocateMem()' did not provide a valid memory block
     */
    ManagedArray(
        size_t nElem         ///< number of elements in the array (NOT number of bytes!!)
        )
      : ManagedArray{}   // delegate basic initialization to the default ctor
    {
      if (nElem == 0) return;

      // actually allocate the memory.
      // Do not catch any exceptions, leave that to the caller
      ptr = allocateMem(nElem);

      if (ptr == nullptr)
      {
        throw std::runtime_error("ManagedArray: couldn't allocate memory for array!");
      }

      cnt = nElem;
      owning = true;
    }

    /** \brief Ctor from the raw pointer of a previously allocated array of which we can, optionally, take ownership
     *
     * \warning Arrays that we take ownership of have to be created with anything that is compatible
     * with the `releaseMem()`-function because we're applying this custom function for releasing the provided pointer.
     *
     * \throws std::invalid_argument if the pointer is not `nullptr` and the number of elements is zero.
     *
     * \throws std::invalid_argument if the pointer is `nullptr` and the number of elements is not zero.
     *
     * If we're not taking ownership, it doesn't matter how the array has been created because
     * we're not calling `releaseMem()' on the pointer.
     *
     */
    ManagedArray(
        T* p,      ///< pointer to a previously allocated array
        size_t n,  ///< number of ELEMENTS (**not Bytes**) in the array
        bool takeOwnership   ///< if set to `true`, this class will dispose the memory after usage with `delete[]`
        )
      :ManagedArray{}
    {
      if (((p == nullptr) && (n > 0))   ||   ((p != nullptr) && (n == 0)))
      {
        throw std::invalid_argument("ManagedArray: inconsistent ctor parameters");
      }

      overwritePointer(p, n, takeOwnership);
    }

    /** \brief Dtor; releases the memory if we're owning it
     */
    virtual ~ManagedArray()
    {
      if ((ptr != nullptr) && owning)
      {
        releaseMem(ptr);
        ptr = nullptr;
        cnt = 0;
      }
    }

    /** \brief Copy ctor; creates a DEEP COPY of an existing array.
     *
     * \throws std::runtime_error if the memory allocation failed
     */
    ManagedArray(
        const ManagedArray& other   ///< the array containing the data to be copied
        )
      :ManagedArray{other.view()}
    {
    }

    /** \brief Copy ctor; creates a DEEP COPY of an existing array.
     *
     * \throws std::runtime_error if the memory allocation failed
     */
    ManagedArray(
        const ArrayView<T>& other   ///< the array containing the data to be copied
        )
      :ManagedArray{other.size()}
    {
      memcpy(to_voidPtr(), other.to_voidPtr(), byteSize());
    }

    /** \brief Disabled copy assignment operator.
     *
     * If I need copy assignment, I want to make it explicit using
     * the copy constructor.
     */
    ManagedArray& operator= (const ManagedArray& other) = delete;

    /** \brief Move ctor
     */
    ManagedArray(
        ManagedArray&& other   ///< the ManagedArray whose content shall be transfered to this instance
        ) noexcept
    {
      // take over the other's state
      overwritePointer(other.ptr, other.cnt, other.owning);

      // clear the other's state
      other.overwritePointer(nullptr, 0, false);
    }

    /** \brief Move assignment
     */
    ManagedArray<T>& operator = (ManagedArray<T>&& other)
    {
      // free currently owned resources
      if (owning && (ptr != nullptr)) releaseMem(ptr);

      // take over the other's state
      overwritePointer(other.ptr, other.cnt, other.owning);

      // clear the other's state
      other.overwritePointer(nullptr, 0, false);

      return *this;
    }

    /** \brief Comparison of ManagedArrays
     *
     * Two genuine ManagedArrays can never be identical:
     *   * if freshly created, a new pointer is allocated
     *   * if created via copy ctor, a new pointer / new array for the target is allocated
     *   * upon move, the source pointer is overwritte with zero.
     *
     * The only exception is if the user has constructed two arrays with
     * the same (raw) pointer. This is BAAAD, because a resource should
     * only be owned by one instance.
     *
     * The comparison only checks the pointer and size, not the "owning" flag.
     * Thus, an owning and a not-owning array pointing at the same memory
     * location are considered equal.
     *
     * \returns `true` if the two arrays point at the same location and have the same size;
     * `false` otherwise.
     */
    bool operator ==(const ManagedArray<T>& other) noexcept
    {
      return ((ptr = other.ptr) && (cnt == other.cnt));
    }

    /** \brief Comparison of ManagedArrays, inverted
     *
     * The comparison only checks the pointer and size, not the "owning" flag.
     *
     * \returns `false` if the two arrays point at the same location and have the same size;
     * `true` otherwise.
     */
    bool operator !=(const ManagedArray<T>& other) noexcept
    {
      return ((ptr != other.ptr) || (cnt != other.cnt));
    }

    /** \returns the number of elements in the array
     *
     * \note The return value is usually NOT IDENTICAL with the number of BYTES allocated by the array!
     */
    size_t size() const
    {
      return cnt;
    }

    /** \returns the number of bytes in the array
     *
     * \note The return value is usually NOT IDENTICAL with the number of ELEMENTS the array!
     */
    size_t byteSize() const
    {
      return cnt * sizeof(T);
    }

    /** \brief Provides read/write access to an element in the array
     *
     * \throws std::out_of_range if the index is outside the array's bounds
     */
    T& operator[](
        size_t idx   ///< the index of the item to access
        ) const
    {
      if (idx >= cnt)
      {
        throw std::out_of_range("ManagedArray: out-of-bounds access");
      }

      return ptr[idx];
    }

    /** \returns a read/write ference to the first element in the array
     *
     * \throws std::out_of_range if the array is empty
     */
    T& first() const
    {
      if ((cnt == 0) || (ptr == nullptr))
      {
        throw std::out_of_range("ManagedArray: attempt to access the first element of an empty array");
      }

      return ptr[0];
    }

    /** \returns a read/write reference to the last element in the array
     *
     * \throws std::out_of_range if the array is empty
     */
    T& last() const
    {
      if ((cnt == 0) || (ptr == nullptr))
      {
        throw std::out_of_range("ManagedArray: attempt to access the last element of an empty array");
      }

      return ptr[cnt - 1];
    }

    /** \returns an ArrayView for this array
     */
    ArrayView<T> view() const
    {
      return ArrayView<T>(ptr, cnt);
    }

    /** \brief Releases the currently managed memory if we're owning it
     *
     * \throws std::runtime_error if an attempt was made to release memory that we're not owning
     */
    void releaseMemory()
    {
      // do nothing if we don't manage any memory
      if ((ptr == nullptr) && (cnt == 0)) return;

      if (!owning)
      {
        throw std::runtime_error("ManagedArray: attempt to release not-owned memory");
      }

      if (ptr != nullptr)
      {
        releaseMem(ptr);
        ptr = nullptr;
        cnt = 0;
      }
    }

    /** \returns `true` if the array contains elements, `false` otherwise
     */
    bool notEmpty() const
    {
      return (cnt > 0);
    }

    /** \returns `true` if the array is empty, `false` otherwise
     */
    bool empty() const
    {
      return (cnt == 0);
    }

    /** \returns the array's base pointer casted to `char*`
     */
    char* to_charPtr() const
    {
      return reinterpret_cast<char *>(ptr);
    }

    /** \returns the array's base pointer casted to `void*`
     */
    void* to_voidPtr() const
    {
      return reinterpret_cast<void *>(ptr);
    }

    /** \returns the array's base pointer casted to `uint8*`
     */
    uint8_t* to_uint8Ptr() const
    {
      return reinterpret_cast<uint8_t *>(ptr);
    }

    /** \returns the array's base pointer casted to `unsigned char*`
     */
    unsigned char* to_ucPtr() const
    {
      return reinterpret_cast<unsigned char *>(ptr);
    }

    /** \brief Resizes the array to a new number of elements.
     *
     * If the new size is larger than the old size, the content
     * of the additional memory is undefined.
     *
     * If the new size is zero, the currently allocated memory is released if we're owning it.
     *
     * Unless the new size is zero or it equals the current size, this operation
     * involves the allocation of a new memory block and copying of data between
     * the old and the new memory.
     *
     * In case that we're **not owning** the memory and the new size is different
     * than the existing size, the function will fail with an execption.
     *
     * \throws std::bad_alloc if the required memory could not be allocated
     *
     * \throws std::runtime_error if we were requested to resize an array that we're not owning
     */
    void resize(
        size_t newSize   ///< the new number of elements (NOT BYTES!) in the array
        )
    {
      if (newSize == cnt) return;  // nothing to do

      if (!owning)
      {
        throw std::runtime_error("ManagedArray: attempt to resize a not-owned array");
      }

      if (newSize == 0)
      {
        releaseMemory();
        return;
      }

      // allocate new memory
      T* tmpPtr = allocateMem(newSize);
      if (tmpPtr == nullptr)
      {
        throw std::bad_alloc();
      }

      // copy contents over, if any
      if (ptr != nullptr)
      {
        size_t nCopyElements = (newSize > cnt) ? cnt : newSize;
        void* srcPtr = to_voidPtr();
        void* dstPtr = reinterpret_cast<void *>(tmpPtr);
        memcpy(dstPtr, srcPtr, nCopyElements * sizeof(T));

        // release the current memory
        releaseMem(ptr);
      }

      overwritePointer(tmpPtr, newSize, true);
    }

    /** \returns `true` if we're owning the array's memory, `false` otherwise
     */
    bool isOwning() const { return owning; }

    /** \brief Copies data from another array into this array.
     *
     * \throws std::out_of_range if the source data does not completely fit into the memory
     */
    void copyOver(
        const ArrayView<T>& src,   ///< the data that shall be copied into our array
        size_t idxFirstDstElem=0   ///< the start index in our array for the data
        )
    {
      if ((idxFirstDstElem + src.size()) > cnt)
      {
        throw std::out_of_range("ManagedArray: copyOver would exceed array limits");
      }

      void* dstPtr = to_voidPtr() + idxFirstDstElem * sizeof(T);
      memcpy(dstPtr, src.to_voidPtr(), src.byteSize());
    }

  protected:
    /** \brief An internal function that can be used by derived classes
     * to unconditionally overwrite the currently managed array.
     *
     * \warning Calling this function does not release any memory, even
     * if we were owning the memory! The caller has to ensure that the
     * previously owned memory has been released properly!
     */
    void overwritePointer(
        T* newPtr,   ///< the new base pointer for the array
        size_t newSize,    ///< the new number of elements (**not bytes**) in the array
        bool hasOwnership   ///< set to true if we shall take ownership of the memory
        ) noexcept
    {
      ptr = newPtr;
      if (ptr == nullptr)
      {
        cnt = 0;
        owning = false;
      } else {
        cnt = newSize;
        owning = hasOwnership;
      }
    }

    /** \brief Allocation function that has to be overridden by a derived class.
     *
     * Any exceptions that this function throws will be re-thrown by e.g. the
     * ctor of the derived class.
     *
     * \returns a pointer to the newly allocated memory or `nullptr` to indicate an error
     */
    virtual T* allocateMem(
        size_t nElem   ///< number of elements (**not bytes**) to allocate memory for
        )
    {
      return new T[nElem]();
    }

    /** \brief De-allocation function for freeing memory that has previously been
     * allocated with `allocateMem()'.
     *
     * Has to be overriden by derived classes.
     *
     * Will be called from the dtor and should thus preferably not throw exceptions
     */
    virtual void releaseMem (
        T* p   ///< pointer to the memory section that should be freed; **can be `nullptr`**
        )
    {
      if (p != nullptr) delete[] p;
    }

  private:
    T* ptr{nullptr};
    size_t cnt{0};
    bool owning{false};
  };

  //----------------------------------------------------------------------------

  /** \brief A specialized ManagedArray for memory segments; uses UI8 (==> bytes) as internal format
   *
   * Offers the additional benefit of being constructable by COPYING data from a MemView.
   */
  class MemArray : public ManagedArray<uint8_t>
  {
    // Inherit base constructors
    using ManagedArray<uint8_t>::ManagedArray;

  public:
    /** \brief Ctor that creates a DEEP COPY of a MemView.
     *
     * \throws std::runtime_error if the memory allocation failed
     */
    explicit MemArray(const MemView& v)
      :ManagedArray<uint8_t>{v.byteSize()}
    {
      memcpy(to_voidPtr(), v.to_voidPtr(), byteSize());
    }

    /** \brief Convenience ctor from a plain, old char pointer; data IS NOT COPIED
     *
     * \warning We **do not** take ownership of this pointer because we can't be sure about its origin
     */
    MemArray(
        char* p,   ///< pointer to a previously allocated array
        size_t n  ///< array size in bytes
        )
      :MemArray(reinterpret_cast<uint8_t*>(p), n, false) {}

    /** \brief Convenience ctor from a plain, old void pointer; data IS NOT COPIED
     *
     * \warning We **do not** take ownership of this pointer because we can't be sure about its
     * origin and since we're coming from a `void`-pointer that could literally point to anything.
     */
    MemArray(
        void* p,   ///< pointer to a previously allocated array in memory
        size_t n  ///< array size in bytes
        )
      :MemArray(reinterpret_cast<uint8_t*>(p), n, false) {}

    /** \returns a `MemView` instance representing this MemArray
     */
    MemView view() const
    {
      return MemView{to_ucPtr(), size()};
    }

    /** \brief Move assignmemt
     *
     * Delegates the call to the parent's move assignment operator.
     */
    MemArray& operator=(MemArray&& other)
    {
      return static_cast<MemArray&>(ManagedArray<uint8_t>::operator =(std::move(other)));
    }

    /** \brief Move ctor
     *
     * Delegates the call to the parent's move ctor.
     */
    MemArray (MemArray&& other)
      :ManagedArray<uint8_t>(std::move(other)) {}

    /** \brief Copy ctor
     *
     * Delegates the call to the parent's copy ctor.
     */
    MemArray (const MemArray& other)
      :ManagedArray<uint8_t>(other) {}

    /** \brief Disabled copy assignment operator
     *
     * If you need copy assignment, make it explicit using the copy ctor
     */
    MemArray& operator =(const MemArray& other) = delete;
  };

}

#endif
