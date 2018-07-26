#pragma once

#include "string.h" //memmove
#include "stdlib.h" //malloc
#include <type_traits> //enable_if, is_trivial

namespace sdh
{
   template <typename T>
   class DynamicArray
   {
   public:
      typedef T* iterator;
      typedef T const * const_iterator;

      DynamicArray()
      {
         m_first = m_last = m_capacity = nullptr;
      }
      DynamicArray(size_t count, T const& obj)
      {
         m_first = m_last = m_capacity = nullptr;
         while (count--) push_back(obj);
      }
      explicit DynamicArray(size_t count)
      {
         m_first = m_last = m_capacity = nullptr;

         reserve(count);
         m_last = m_first + count;
         _construct_range(m_first, m_last);
      }
      DynamicArray(std::initializer_list<T> list)
      {
         m_first = m_last = m_capacity = nullptr;
         for (auto&& obj : list) push_back(std::move(obj));
      }
      template <typename InputIt>
      DynamicArray(InputIt i1, InputIt i2)
      {
         m_first = m_last = m_capacity = nullptr;
         while (i1 != i2) push_back(std::move(*i1++));
      }
      DynamicArray(DynamicArray&& rhs)
      {
         m_first = rhs.m_first;
         m_last = rhs.m_last;
         m_capacity = rhs.m_capacity;

         rhs.m_first = rhs.m_last = rhs.m_capacity = nullptr;
      }
      DynamicArray(DynamicArray const& rhs)
      {
         m_first = m_last = m_capacity = nullptr;
         reserve(rhs.size());
         for (auto&& elem : rhs) push_back(elem);
      }

      DynamicArray& operator=(DynamicArray&& rhs)
      {
         if (this == &rhs) return *this;

         clear();
         m_first = rhs.m_first;
         m_last = rhs.m_last;
         m_capacity = rhs.m_capacity;

         rhs.m_first = rhs.m_last = rhs.m_capacity = nullptr;
         return *this;
      }
      DynamicArray operator=(DynamicArray const& rhs)
      {
         if (this == &rhs) return *this;

         clear();
         reserve(rhs.size());
         for (auto&& elem : rhs) push_back(elem);

         return *this;
      }

      ~DynamicArray()
      {
         clear();
         free(m_first);
      }
      void swap(DynamicArray& rhs)
      {
         auto temp = m_first;
         m_first = rhs.m_first;
         rhs.m_first = temp;

         temp = m_last;
         m_last = rhs.m_last;
         rhs.m_last = temp;

         temp = m_capacity;
         m_capacity = rhs.m_capacity;
         rhs.m_capacity = temp;
      }
      void clear()
      {
         _clear_impl(m_first, m_last);
      }
      void push_back(T&& newElem)
      {
         if (m_last == m_capacity) _grow();
         new (m_last++) T(std::move(newElem));
      }
      void push_back(T const& newElem)
      {
         if (m_last == m_capacity) _grow();
         new (m_last++) T(newElem);
      }
      template <class... Args>
      void emplace_back(Args&&... args)
      {
         if (m_last == m_capacity) _grow();
         new (m_last++) T(std::forward<Args>(args)...);
      }
      void insert(iterator where, T&& newElem)
      {
         if (m_last == m_capacity)
         {
            auto idx = where - m_first;
            _grow();
            where = m_first + idx;
         }

         auto it = m_last++;
         auto prev = it - 1;
         _move_range_backwards(it, prev, it - where);

         new (it) T(std::move(newElem));
      }
      void insert(iterator where, const T& newElem)
      {
         if (m_last == m_capacity)
         {
            auto idx = where - m_first;
            _grow();
            where = m_first + idx;
         }

         auto it = m_last++;
         auto prev = it - 1;
         _move_range_backwards(it, prev, it - where);

         new (it) T(newElem);
      }
      template <typename InputIt>
      void insert(iterator where, InputIt first, InputIt last)
      {
         if (first == last) return;

         auto count = last - first;
         if (m_last + count > m_capacity)
         {
            auto idx = where - m_first;
            _grow(size() + count);
            where = m_first + idx;
         }

         auto it = m_last + count - 1;
         auto prev = it - count;
         m_last += count;

         _move_range_backwards(it, prev, it - (where + count - 1));

         while (where != it + 1) new (where++) T(std::move(*first++));
      }
      void pop_back()
      {
         (--m_last)->~T();
      }

      template <typename InputIt>
      void assign(InputIt i1, InputIt i2)
      {
         clear();
         while (i1 != i2) push_back(std::move(*i1++));
      }
      void assign(std::initializer_list<T> list)
      {
         clear();
         for (auto&& obj : list) push_back(std::move(obj));
      }


      T& operator[](size_t idx) { return m_first[idx]; }
      T const& operator[](size_t idx) const { return m_first[idx]; }

      T& front()
      {
         return *m_first;
      }
      T const& front() const
      {
         return *m_first;
      }
      T& back()
      {
         return *(m_last - 1);
      }
      T const& back() const
      {
         return *(m_last - 1);
      }

      iterator begin()
      {
         return m_first;
      }
      const_iterator begin() const
      {
         return m_first;
      }
      const_iterator begin_const() const
      {
         return m_first;
      }
      iterator end()
      {
         return m_last;
      }
      const_iterator end() const
      {
         return m_last;
      }
      const_iterator end_const() const
      {
         return m_last;
      }
      size_t size() const
      {
         return m_last - m_first;
      }
      T* data()
      {
         return m_first;
      }
      T const* data() const
      {
         return m_first;
      }
      bool empty() const
      {
         return m_first == m_last;
      }
      void erase(iterator it)
      {
         auto next = it + 1;
         _clear_impl(it, next);
         --m_last;
         next = it + 1;
         _move_range(it, next, m_last - it);
      }
      void erase(iterator first, iterator last)
      {
         if (first == last) return;

         auto it = last;
         _clear_impl(first, it);
         _move_range(first, last, m_last - last);
         m_last -= last - first;
      }
      void reserve(size_t newCount)
      {
         if (newCount > capacity()) _grow(newCount);
      }
      void resize(size_t newCount)
      {
         auto sz = (size_t)(m_last - m_first);
         if (newCount <= sz)
         {
            auto newLast = m_first + newCount;
            _clear_impl(newLast, m_last);
            return;
         }
         reserve(newCount);
         auto toAdd = newCount - sz;

         auto prevLast = m_last;
         m_last += toAdd;
         _construct_range(prevLast, m_last);
      }
      void resize(size_t newCount, T const& toConstruct)
      {
         auto sz = (size_t)(m_last - m_first);
         if (newCount <= sz)
         {
            auto newLast = m_first + newCount;
            _clear_impl(newLast, m_last);
            return;
         }
         reserve(newCount);
         auto toAdd = newCount - sz;
         while (toAdd--) push_back(toConstruct);
      }
      size_t capacity() const
      {
         return m_capacity - m_first;
      }
      void shrink_to_fit()
      {
         if (m_last == m_capacity) return;

         auto newSz = m_last - m_first;

         _grow_impl(m_first, m_last, m_capacity, newSz);
      }
   private:
      void _grow(size_t reqSize)
      {
         if (!m_capacity)
         {
            size_t sz = 8;
            while (sz < reqSize) sz <<= 1;
            m_last = m_first = (T*)malloc(sizeof(T) * sz);
            m_capacity = m_first + sz;
            return;
         }

         auto newSz = (size_t)(m_capacity - m_first) << 1;
         while (newSz < reqSize) newSz <<= 1;

         _grow_impl(m_first, m_last, m_capacity, newSz);
      }
      void _grow()
      {
         if (!m_capacity)
         {
            m_last = m_first = (T*)malloc(sizeof(T) * 8);
            m_capacity = m_first + 8;
            return;
         }

         auto newSz = (size_t)(m_capacity - m_first) << 1;

         _grow_impl(m_first, m_last, m_capacity, newSz);
      }

      template<typename U>
      typename std::enable_if<true == std::is_trivial<U>::value>::type _clear_impl(U*& first, U*& last)
      {
         last = first;
      }

      template<typename U>
      typename std::enable_if<false == std::is_trivial<U>::value>::type _clear_impl(U*& first, U*& last)
      {
         while (first != last) (--last)->~U();
      }


      template<typename U>
      typename std::enable_if<true == std::is_trivial<U>::value>::type _move_range(U*& dst, U*& src, size_t count)
      {
         memmove(dst, src, count * sizeof(U));
         src += count;
         dst += count;
      }

      template<typename U>
      typename std::enable_if<false == std::is_trivial<U>::value>::type _move_range(U*& dst, U*& src, size_t count)
      {
         while (count--)
         {
            new (dst++) T(std::move(*(src)));
            (src++)->~T();
         }
      }

      template<typename U>
      typename std::enable_if<true == std::is_trivial<U>::value>::type _move_range_backwards(U*& dst, U*& src, size_t count)
      {
         if (!count) return;

         memmove(dst - (count - 1), src - (count - 1), count * sizeof(U));
         src -= count;
         dst -= count;
      }

      template<typename U>
      typename std::enable_if<false == std::is_trivial<U>::value>::type _move_range_backwards(U*& dst, U*& src, size_t count)
      {
         while (count--)
         {
            new (dst--) T(std::move(*src));
            (src--)->~T();
         }
      }


      template<typename U>
      typename std::enable_if<true == std::is_trivial<U>::value>::type _grow_impl(U*& first, U*& last, U*& capacity, size_t newSz)
      {
         auto oldArray = first;
         first = (U*)realloc(first, sizeof(U) * newSz);
         last = first + (last - oldArray);
         capacity = first + newSz;
      }

      template<typename U>
      typename std::enable_if<false == std::is_trivial<U>::value>::type _grow_impl(U*& first, U*& last, U*& capacity, size_t newSz)
      {
         auto oldArray = first;
         first = (U*)malloc(sizeof(U) * newSz);
         auto target = first;
         for (auto it = oldArray; it < m_last; ++it) new (target++) U(std::move(*it));
         last = target;
         capacity = first + newSz;
         free(oldArray);
      }

      template<typename U>
      typename std::enable_if<true == std::is_trivial<U>::value>::type _construct_range(U* first, U* last)
      {
      }

      template<typename U>
      typename std::enable_if<false == std::is_trivial<U>::value>::type _construct_range(U* first, U* last)
      {
         while (first != last) new (first++) U;
      }

      T* m_first, *m_last, *m_capacity;
   };


   //these free-function begin and end make this work everywhere with C++ argument-dependent lookup
   //this means you can do begin(array) with no namespace and it just works and doesn't conflict with std::begin().
   template <typename T>
   T* begin(DynamicArray<T>& array)
   {
      return array.begin();
   }

   template <typename T>
   T const* begin(DynamicArray<T> const& array)
   {
      return array.begin();
   }

   template <typename T>
   T const* begin_const(DynamicArray<T> const& array)
   {
      return array.begin();
   }

   template <typename T>
   T* end(DynamicArray<T>& array)
   {
      return array.end();
   }

   template <typename T>
   T const* end(DynamicArray<T> const& array)
   {
      return array.end();
   }

   template <typename T>
   T const* end_const(DynamicArray<T> const& array)
   {
      return array.end();
   }
}

//you don't actually want to use the namespace.  This pulls it into the global namespace, but ADL still works!
using sdh::DynamicArray;