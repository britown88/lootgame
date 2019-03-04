#pragma once

#include <type_traits>
#include <iterator>


template <typename T>
class Array
{
public:
   typedef T* pointer;
   typedef T& reference;
   typedef const reference const_reference;
   typedef const pointer const_pointer;
   typedef pointer iterator;
   typedef const_pointer const_iterator;
   typedef std::reverse_iterator<iterator> reverse_iterator;
   typedef std::reverse_iterator<const_iterator> reverse_const_iterator;

   Array() noexcept {}
   explicit Array(size_t count) noexcept
   {
      resize(count);
   }
   Array(size_t count, T const& value) noexcept
   {
      resize(count, value);
   }
   template <typename InputIt>
   Array(InputIt first, InputIt last) noexcept
   {
      insert(_last, first, last);
   }
   Array(Array&& rhs) noexcept
   {
      _first = rhs._first;
      _last = rhs._last;
      _capacity = rhs._capacity;
      rhs._first = rhs._last = rhs._capacity = nullptr;
   }
   Array(Array const& rhs) noexcept
   {
      auto sz = rhs.size();
      reserve(sz);
      _last = _first + sz;
      _constructCopyRange(_first, _last, rhs.begin());
   }
   Array(std::initializer_list<T> init)
   {
      insert(_last, init.begin(), init.end());
   }
   Array& operator=(Array&& rhs) noexcept
   {
      if (this == &rhs) return *this;
      clear();
      free(_first);
      _first = rhs._first;
      _last = rhs._last;
      _capacity = rhs._capacity;
      rhs._first = rhs._last = rhs._capacity = nullptr;
      return *this;
   }
   Array& operator=(Array const& rhs) noexcept
   {
      if (this == &rhs) return *this;
      clear();
      auto sz = rhs.size();
      reserve(sz);
      _last = _first + sz;
      _constructCopyRange(_first, _last, rhs.begin());
      return *this;
   }
   Array& operator=(std::initializer_list<T> init)
   {
      clear();
      insert(_last, init.begin(), init.end());
      return *this;
   }
   Array& assign(size_t count, const T& value) noexcept
   {
      clear();
      resize(count, value);
      return *this;
   }
   template <typename InputIt>
   Array& assign(InputIt first, InputIt last) noexcept
   {
      clear();
      insert(_last, first, last);
      return *this;
   }
   Array& assign(std::initializer_list<T> init)
   {
      clear();
      insert(_last, init.begin(), init.end());
      return *this;
   }
   ~Array()
   {
      clear();

#ifdef IS_PLUGIN
      if (pal_memory_memory && freeFn) free(_first);
#elif IS_LIBRARY
      if (CONTEXT) free(_first);
#else 
      free(_first);
#endif

   }

   iterator begin()
   {
      return _first;
   }
   const_iterator begin() const
   {
      return _first;
   }
   reverse_iterator rbegin()
   {
      return std::make_reverse_iterator(end());
   }
   reverse_const_iterator rbegin() const
   {
      return std::make_reverse_iterator(end());
   }
   const_iterator cbegin() const
   {
      return _first;
   }

   iterator end()
   {
      return _last;
   }
   const_iterator end() const
   {
      return _last;
   }
   const_iterator cend() const
   {
      return _last;
   }
   reverse_iterator rend()
   {
      return std::make_reverse_iterator(begin());
   }
   reverse_const_iterator rend() const
   {
      return std::make_reverse_iterator(begin());
   }
   void clear()
   {
      _destroyRange(_first, _last);
      _last = _first;
   }
   pointer data()
   {
      return _first;
   }
   const_pointer data() const
   {
      return _first;
   }
   bool empty() const
   {
      return _first == _last;
   }
   size_t size() const
   {
      return _last - _first;
   }
   size_t capacity() const
   {
      return _capacity - _first;
   }
   void push_back(T&& val)
   {
      if (_last == _capacity) _grow(_nextCapacity(size() + 1));
      _constructMove(_last, std::move(val));
      ++_last;
   }
   void push_back(T const& val)
   {
      if (_last == _capacity) _grow(_nextCapacity(size() + 1));
      _constructCopyRange(_last, _last + 1, val);
      ++_last;
   }
   void push_back_array(Array<T>&& val)
   {
      insert(end(), std::make_move_iterator(val.begin()), std::make_move_iterator(val.end()));
      val.clear();
   }
   void push_back_array(Array<T> const& val)
   {
      insert(end(), val.begin(), val.end());
   }
   const_iterator erase(const_iterator first, const_iterator last)
   {
      _moveRange(first, last, _last);
      auto newLast = _last - (last - first);
      _destroyRange(newLast, _last);
      _last = newLast;
      return first;
   }
   const_iterator erase(const_iterator first)
   {
      return erase(first, first + 1);
   }
   void reserve(size_t size)
   {
      size_t cap = capacity();
      if (size < cap) return;
      _grow(size);
   }

   void resize(size_t newSize)
   {
      size_t sz = size();
      if (newSize < sz)
      {
         erase(_first + newSize, _last);
         return;
      }

      //making it larger...
      if (newSize > capacity()) _grow(_nextCapacity(newSize));
      _constructRange(_last, _first + newSize);
      _last = _first + newSize;
   }
   void resize(size_t newSize, T const& value)
   {
      size_t sz = size();
      if (newSize < sz)
      {
         erase(_first + newSize, _last);
         return;
      }

      //making it larger...
      if (newSize > capacity()) _grow(_nextCapacity(newSize));
      _constructCopyRange(_last, _first + newSize, value);
      _last = _first + newSize;
   }


   void ensure_size(size_t newSize)
   {
      size_t sz = size();
      if (newSize <= sz) return;

      //making it larger...
      if (newSize > capacity()) _grow(_nextCapacity(newSize));
      _constructRange(_last, _first + newSize);
      _last = _first + newSize;
   }
   void ensure_size(size_t newSize, T const& value)
   {
      size_t sz = size();
      if (newSize <= sz) return;

      //making it larger...
      if (newSize > capacity()) _grow(_nextCapacity(newSize));
      _constructCopyRange(_last, _first + newSize, value);
      _last = _first + newSize;
   }
   void reset()
   {
      clear();
      free(_first);
      _first = _last = _capacity = nullptr;
   }
   void shrink_to_fit()
   {
      if (empty())
      {
         free(_first);
         _first = _last = _capacity = nullptr;
      }
      else
      {
         _grow(size());
      }
   }
   T& operator[](size_t idx)
   {
#ifdef ARRAY_DEBUG
      assert(_first + idx < _last); // Out of bounds
#endif

      return _first[idx];
   }
   T const& operator[](size_t idx) const
   {
#ifdef ARRAY_DEBUG
      assert(_first + idx < _last); // Out of bounds
#endif
      return _first[idx];
   }
   T& at(size_t idx)
   {
#ifdef ARRAY_DEBUG
      assert(_first + idx < _last); // Out of bounds
#endif
      return _first[idx];
   }
   T const& at(size_t idx) const
   {
#ifdef ARRAY_DEBUG
      assert(_first + idx < _last); // Out of bounds
#endif
      return _first[idx];
   }
   T& front()
   {
      return *_first;
   }
   T const& front() const
   {
      return *_first;
   }
   T& back()
   {
      return *(_last - 1);
   }
   T const& back() const
   {
      return *(_last - 1);
   }
   //requires malloc'd data.
   void from_existing(T* first, T* last)
   {
      clear();
      free(_first);
      _first = first;
      _last = _capacity = last;
   }
   //requires malloc'd data.
   void from_existing(T* first, size_t count)
   {
      from_existing(first, first + count);
   }
   iterator insert(const_iterator pos, const T& value)
   {
      if (pos == end())
      {
         push_back(value);
         return end() - 1;
      }

      auto outIter = _moveDataUpFrom(pos, 1);
      *outIter = value;

      return outIter;
   }
   iterator insert(const_iterator pos, T&& value)
   {
      if (pos == end())
      {
         push_back(std::move(value));
         return end() - 1;
      }

      auto outIter = _moveDataUpFrom(pos, 1);
      *outIter = std::move(value);

      return outIter;
   }
   iterator insert(const_iterator pos, size_t count, const T& value)
   {
      if (pos == end())
      {
         size_t sz = size();
         resize(sz + count, value);
         return begin() + sz;
      }

      auto outIter = _moveDataUpFrom(pos, count);
      auto iterEnd = outIter + count;
      for (auto itr = outIter; itr != iterEnd; ++itr) *itr = value;

      return outIter;
   }
   template <typename InputIt>
   iterator insert(const_iterator pos, InputIt first, InputIt last)
   {
      //this has optimized forms depending on the type of the iterator.
      typedef typename std::iterator_traits<InputIt>::iterator_category tag;
      return insert(pos, first, last, tag{});
   }
   template <typename InputIt, typename ItTag>
   iterator insert(const_iterator pos, InputIt first, InputIt last, ItTag)
   {
      //insert into the end, and then rotate it up front.
      auto offset = pos - _first;
      auto startSz = size();
      while (first != last) push_back(*first++);
      auto it = _first + offset;
      std::rotate(it, _first + startSz, _last);
      return it;
   }
   template <typename InputIt>
   iterator insert(const_iterator pos, InputIt first, InputIt last, std::random_access_iterator_tag)
   {
      //we're inserting a random access iterator, so we can do some optimizations.
      auto newElems = std::distance(first, last);
      auto offset = pos - _first;
      reserve(size() + newElems);
      auto sz = size();
      if (offset == sz)
      {
         //todo: this could be a bit more efficient directly using range calls.
         while (first != last) push_back(*first++);
         return _first + sz;
      }

      auto outIter = _first + offset;
      _constructRange(_last, _last + newElems);
      _moveRange(outIter + newElems, outIter, _last);
      _last = _last + newElems;
      auto writeIter = outIter;
      while (first != last) *writeIter++ = (*first++);
      return outIter;
   }
   iterator insert(const_iterator pos, std::initializer_list<T> ilist)
   {
      return insert(pos, ilist.begin(), ilist.end());
   }

   template< class... Args >
   iterator emplace(const_iterator pos, Args&&... args)
   {
      if (pos == end())
      {
         emplace_back(std::forward<Args>(args)...);
         return end() - 1;
      }

      auto outIter = _moveDataUpFrom(pos, 1);
      _destroyRange(outIter, outIter + 1);
      new (outIter) T(std::forward<Args>(args)...);

      return outIter;
   }

   template< class... Args >
   reference emplace_back(Args&&... args)
   {
      if (_last == _capacity) _grow(_nextCapacity(size() + 1));
      new (_last++) T(std::forward<Args>(args)...);
      return back();
   }

   void pop_back()
   {
      _destroyRange(_last - 1, _last);
      --_last;
   }

   T* release()
   {
      auto out = _first;
      _first = _last = _capacity = nullptr;
      return out;
   }

   void swap(Array& other) noexcept
   {
      std::swap(other._first, _first);
      std::swap(other._last, _last);
      std::swap(other._capacity, _capacity);
   }

   bool operator==(Array const& rhs) const
   {
      size_t sz = size();
      if (sz != rhs.size()) return false;
      for (size_t i = 0; i < sz; ++i)
      {
         if (_first[i] != rhs._first[i]) return false;
      }
      return true;
   }
   bool operator!=(Array const& rhs) const
   {
      return !(*this == rhs);
   }
   bool operator<(Array const& rhs) const
   {
      //lexicographically compare...
      size_t szLhs = size();
      size_t szRhs = rhs.size();

      size_t sz = szLhs;
      if (szRhs < sz) sz = szRhs;
      for (size_t i = 0; i < sz; ++i)
      {
         if (_first[i] < rhs._first[i]) return true;
         if (_first[i] > rhs._first[i]) return false;
      }

      return szLhs < szRhs;
   }
   bool operator<=(Array const& rhs) const
   {
      //lexicographically compare...
      size_t szLhs = size();
      size_t szRhs = rhs.size();

      size_t sz = szLhs;
      if (szRhs < sz) sz = szRhs;
      for (size_t i = 0; i < sz; ++i)
      {
         if (_first[i] < rhs._first[i]) return true;
         if (_first[i] > rhs._first[i]) return false;
      }

      return szLhs < szRhs || szLhs == szRhs;
   }
   bool operator>(Array const& rhs) const
   {
      return (rhs < *this);
   }
   bool operator>=(Array const& rhs) const
   {
      return (rhs <= *this);
   }

private:
   iterator _moveDataUpFrom(const_iterator pos, size_t count)
   {
      auto offset = pos - _first;
      if (_last + count > _capacity) _grow(_nextCapacity(size() + count));
      _last += count;
      _constructRange(_last - count, _last);
      auto outIter = _first + offset;
      _moveRange(outIter + count, outIter, _last - count);

      return outIter;
   }
   size_t _nextCapacity(size_t capacity) const
   {
      if (capacity < 8) return 8;  //todo: is this size sensible?
                                    //todo: replace with leading zero intrinsic
      auto sz = 8;
      while (sz < capacity) sz *= 2;
      return sz;
   }


#define TRIVIAL template <typename Q = T> inline typename std::enable_if<std::is_trivial<Q>::value>::type
#define NOT_TRIVIAL template <typename Q = T> inline typename std::enable_if<!std::is_trivial<Q>::value>::type

   TRIVIAL _grow(size_t newCapacity)
   {
      auto sz = size();
      _first = (T*)realloc(_first, newCapacity * sizeof(T));
      _last = _first + sz;
      _capacity = _first + newCapacity;
   }
   NOT_TRIVIAL _grow(size_t newCapacity)
   {
      auto sz = size();
      auto newPtr = (T*)malloc(newCapacity * sizeof(T));
      _constructMoveRange(newPtr, newPtr + sz, _first);
      free(_first);
      _first = newPtr;
      _last = _first + sz;
      _capacity = _first + newCapacity;
   }
   TRIVIAL _destroyRange(iterator first, iterator last)
   {
   }
   NOT_TRIVIAL _destroyRange(iterator first, iterator last)
   {
      while (last != first) (--last)->~T();
   }

   TRIVIAL _constructRange(iterator first, iterator last)
   {
   }
   NOT_TRIVIAL _constructRange(iterator first, iterator last)
   {
      while (first != last) new (first++) T();
   }


   TRIVIAL _constructMove(iterator first, T&& val)
   {
      *first = std::move(val);
   }
   NOT_TRIVIAL _constructMove(iterator first, T&& val)
   {
      new (first) T(std::move(val));
   }


   TRIVIAL _constructCopyRange(iterator first, iterator last, T const& val)
   {
      while (first != last) *first++ = val;
   }
   NOT_TRIVIAL _constructCopyRange(iterator first, iterator last, T const& val)
   {
      while (first != last) new (first++) T(val);
   }

   TRIVIAL _constructCopyRange(iterator first, iterator last, iterator rangeOther)
   {
      memcpy(first, rangeOther, (last - first) * sizeof(T));
   }
   NOT_TRIVIAL _constructCopyRange(iterator first, iterator last, iterator rangeOther)
   {
      while (first != last) new (first++) T(*rangeOther++);
   }


   TRIVIAL _constructMoveRange(iterator first, iterator last, iterator rangeOther)
   {
      memcpy(first, rangeOther, (last - first) * sizeof(T));
   }
   NOT_TRIVIAL _constructMoveRange(iterator first, iterator last, iterator rangeOther)
   {
      while (first != last)
      {
         new (first++) T(std::move(*rangeOther));
         (rangeOther++)->~T();
      }
   }

   TRIVIAL _moveRange(iterator dst, iterator first, iterator last)
   {
      memmove(dst, first, sizeof(T)*(last - first));
   }
   NOT_TRIVIAL _moveRange(iterator dst, iterator first, iterator last)
   {
      if (dst < first)
      {
         while (first != last) *dst++ = std::move(*first++);
      }
      else
      {
         auto dstLast = dst + (last - first);
         while (last != first) *--dstLast = std::move(*--last);
      }
   }

   T * _first = nullptr, *_last = nullptr, *_capacity = nullptr;

#undef TRIVIAL
#undef NOT_TRIVIAL
};

template <typename T>
typename Array<T>::iterator begin(Array<T>& array)
{
   return array.begin();
}

template <typename T>
typename Array<T>::iterator end(Array<T>& array)
{
   return array.end();
}

template <typename T>
typename Array<T>::const_iterator begin(Array<T> const& array)
{
   return array.begin();
}

template <typename T>
typename Array<T>::const_iterator end(Array<T>const& array)
{
   return array.end();
}

