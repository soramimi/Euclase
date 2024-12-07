
#ifndef UNINITIALIZED_VECTOR_H
#define UNINITIALIZED_VECTOR_H

#include <cstring>
#include <cstdlib>
#include <utility>
#include <new>

template <typename T> class uninitialized_vector {
private:
	struct D {
		size_t capacity = 0;
		size_t count = 0;
		T *array = nullptr;
	} d;
public:
	uninitialized_vector()
	{
	}
	uninitialized_vector(size_t n)
	{
		resize(n);
	}
	uninitialized_vector(uninitialized_vector const &r)
	{
		if (r.d.array) {
			resize(r.d.count);
			memcpy(d.array, r.d.array, sizeof(T) * r.d.count);
		}
	}
	uninitialized_vector(uninitialized_vector &&r)
	{
		d = r.d;
		r.d = {};
	}
	~uninitialized_vector()
	{
		delete[] d.array;
	}
	void operator = (uninitialized_vector const &r)
	{
		d.count = 0;
		if (r.d.array) {
			resize(r.d.count);
			memcpy(d.array, r.d.array, sizeof(T) * r.d.count);
		}
	}
	void operator = (uninitialized_vector &&r)
	{
		d = r.d;
		r.d = {};
	}
	class const_iterator;
	class iterator {
		friend class uninitialized_vector;
		friend class const_iterator;
	private:
		T *ptr;
		T *end;
		int compare(iterator const &r) const
		{
			return ptr - r.ptr;
		}
		bool isnull() const
		{
			return !ptr || ptr == end;
		}
	public:
		iterator(T *p, T *end = nullptr)
			: ptr(p)
			, end(end)
		{
		}
		bool operator == (iterator it) const
		{
			if (isnull() && it.isnull()) return true;
			return compare(it) == 0;
		}
		bool operator != (iterator it) const
		{
			return !operator == (it);
		}
		void operator ++ ()
		{
			if (!isnull()) ptr++;
		}
		void operator ++ (int)
		{
			if (!isnull()) ptr++;
		}
		T &operator * ()
		{
			return *ptr;
		}
		T *operator -> ()
		{
			return ptr;
		}
		iterator operator + (size_t n) const
		{
			return isnull() ? iterator(end, end) : iterator(ptr + n, end);
		}
		iterator operator - (size_t n) const
		{
			return isnull() ? iterator(end, end) : iterator(ptr - n, end);
		}
		bool operator < (iterator const &it) const
		{
			return compare(it) < 0;
		}
		bool operator > (iterator const &it) const
		{
			return compare(it) > 0;
		}
		bool operator <= (iterator const &it) const
		{
			return compare(it) <= 0;
		}
		bool operator >= (iterator const &it) const
		{
			return compare(it) >= 0;
		}
		size_t operator - (iterator const &it) const
		{
			if (!ptr || !it.ptr) return 0;
			return ptr - it.ptr;
		}
	};
	class const_iterator {
		friend class uninitialized_vector;
	private:
		T const *ptr;
		T const *end;
		int compare(const_iterator const &r) const
		{
			return ptr - r.ptr;
		}
		bool isnull() const
		{
			return !ptr || ptr == end;
		}
	public:
		const_iterator(T const *p, T const *end = nullptr)
			: ptr(p)
			, end(end)
		{
		}
		const_iterator(iterator const &it)
			: ptr(it.ptr)
			, end(it.end)
		{
		}
		bool operator == (const_iterator it) const
		{
			if (isnull() && it.isnull()) return true;
			return compare(it) == 0;
		}
		bool operator != (const_iterator it) const
		{
			return !operator == (it);
		}
		void operator ++ ()
		{
			if (!isnull()) ptr++;
		}
		void operator ++ (int)
		{
			if (!isnull()) ptr++;
		}
		T const &operator * () const
		{
			return *ptr;
		}
		T const *operator -> () const
		{
			return ptr;
		}
		iterator operator + (size_t n) const
		{
			return isnull() ? const_iterator(end, end) : const_iterator(ptr + n, end);
		}
		iterator operator - (size_t n) const
		{
			return isnull() ? const_iterator(end, end) : const_iterator(ptr - n, end);
		}
		bool operator < (const_iterator const &it) const
		{
			return compare(it) < 0;
		}
		bool operator > (const_iterator const &it) const
		{
			return compare(it) > 0;
		}
		bool operator <= (const_iterator const &it) const
		{
			return compare(it) <= 0;
		}
		bool operator >= (const_iterator const &it) const
		{
			return compare(it) >= 0;
		}
		size_t operator - (const_iterator const &it) const
		{
			if (!ptr || !it.ptr) return 0;
			return ptr - it.ptr;
		}
	};
	class reverse_iterator {
		friend class uninitialized_vector;
		friend class const_iterator;
	private:
		T *ptr;
		T *end;
		int compare(reverse_iterator const &r) const
		{
			return ptr - r.ptr;
		}
		bool isnull() const
		{
			return !ptr || ptr == end;
		}
	public:
		reverse_iterator(T *p, T *end = nullptr)
			: ptr(p)
			, end(end)
		{
		}
		bool operator == (reverse_iterator it) const
		{
			if (isnull() && it.isnull()) return true;
			return compare(it) == 0;
		}
		bool operator != (reverse_iterator it) const
		{
			return !operator == (it);
		}
		void operator ++ ()
		{
			if (!isnull()) ptr--;
		}
		void operator ++ (int)
		{
			if (!isnull()) ptr--;
		}
		T &operator * ()
		{
			return ptr[-1];
		}
		T *operator -> ()
		{
			return &ptr[-1];
		}
		reverse_iterator operator + (size_t n) const
		{
			return isnull() ? reverse_iterator(end, end) : reverse_iterator(ptr - n, end);
		}
		reverse_iterator operator - (size_t n) const
		{
			return isnull() ? reverse_iterator(end, end) : reverse_iterator(ptr + n, end);
		}
		bool operator < (reverse_iterator const &it) const
		{
			return compare(it) > 0;
		}
		bool operator > (reverse_iterator const &it) const
		{
			return compare(it) < 0;
		}
		bool operator <= (reverse_iterator const &it) const
		{
			return compare(it) >= 0;
		}
		bool operator >= (reverse_iterator const &it) const
		{
			return compare(it) <= 0;
		}
		size_t operator - (reverse_iterator const &it) const
		{
			if (!ptr || !it.ptr) return 0;
			return it.ptr - ptr;
		}
	};
	class const_reverse_iterator {
		friend class uninitialized_vector;
		friend class const_iterator;
	private:
		T const *ptr;
		T const *end;
		int compare(const_reverse_iterator const &r) const
		{
			return ptr - r.ptr;
		}
		bool isnull() const
		{
			return !ptr || ptr == end;
		}
	public:
		const_reverse_iterator(T *p, T *end = nullptr)
			: ptr(p)
			, end(end)
		{
		}
		const_reverse_iterator(reverse_iterator const &r)
			: ptr(r.ptr)
			, end(r.end)
		{
		}
		bool operator == (const_reverse_iterator it) const
		{
			if (isnull() && it.isnull()) return true;
			return compare(it) == 0;
		}
		bool operator != (const_reverse_iterator it) const
		{
			return !operator == (it);
		}
		void operator ++ ()
		{
			if (!isnull()) ptr--;
		}
		void operator ++ (int)
		{
			if (!isnull()) ptr--;
		}
		T const &operator * () const
		{
			return ptr[-1];
		}
		T const *operator -> () const
		{
			return &ptr[-1];
		}
		const_reverse_iterator operator + (size_t n) const
		{
			return isnull() ? const_reverse_iterator(end, end) : const_reverse_iterator(ptr - n, end);
		}
		const_reverse_iterator operator - (size_t n) const
		{
			return isnull() ? const_reverse_iterator(end, end) : const_reverse_iterator(ptr + n, end);
		}
		bool operator < (const_reverse_iterator const &it) const
		{
			return compare(it) > 0;
		}
		bool operator > (const_reverse_iterator const &it) const
		{
			return compare(it) < 0;
		}
		bool operator <= (const_reverse_iterator const &it) const
		{
			return compare(it) >= 0;
		}
		bool operator >= (const_reverse_iterator const &it) const
		{
			return compare(it) <= 0;
		}
		size_t operator - (const_reverse_iterator const &it) const
		{
			if (!ptr || !it.ptr) return 0;
			return it.ptr - ptr;
		}
	};
	T *data()
	{
		return d.array;
	}
	T const *data() const
	{
		return d.array;
	}
	size_t size() const
	{
		return d.count;
	}
	void resize(size_t n)
	{
		if (n > d.capacity) {
			uninitialized_vector newvec;
			newvec.d.capacity = (n + 7) & ~7;
			newvec.d.count = n;
			newvec.d.array = (T *)new char[(sizeof(T) * newvec.d.capacity)];
			memcpy(newvec.d.array, d.array, sizeof(T) * d.count);
			delete[] d.array;
			d = newvec.d;
			newvec.d = {};
		} else {
			d.count = n;
		}
	}
	bool empty() const
	{
		return size() == 0;
	}
	void reserve(size_t n)
	{
		if (d.capacity < n) {
			T *newarr = (T *)new char[(sizeof(T) * n)];
			memcpy(newarr, d.array, sizeof(T) * d.count);
			delete[] d.array;
			d.array = newarr;
			d.capacity = n;
		}
	}
	iterator begin()
	{
		if (d.array) {
			return iterator(d.array, d.array + d.count);
		} else {
			return iterator(0, 0);
		}
	}
	const_iterator begin() const
	{
		if (d.array) {
			return const_iterator(d.array, d.array + d.count);
		} else {
			return const_iterator(0, 0);
		}
	}
	reverse_iterator rbegin()
	{
		if (d.array) {
			return reverse_iterator(d.array + d.count, d.array);
		} else {
			return reverse_iterator(0, 0);
		}
	}
	const_reverse_iterator rbegin() const
	{
		if (d.array) {
			return const_reverse_iterator(d.array + d.count, d.array);
		} else {
			return const_reverse_iterator(0, 0);
		}
	}
	iterator end()
	{
		if (d.array) {
			return iterator(d.array + d.count, d.array + d.count);
		} else {
			return iterator(0, 0);
		}
	}
	const_iterator end() const
	{
		if (d.array) {
			return const_iterator(d.array + d.count, d.array + d.count);
		} else {
			return const_iterator(0, 0);
		}
	}
	reverse_iterator rend()
	{
		if (d.array) {
			return reverse_iterator(d.array, d.array);
		} else {
			return reverse_iterator(0, 0);
		}
	}
	const_reverse_iterator rend() const
	{
		if (d.array) {
			return const_reverse_iterator(d.array, d.array);
		} else {
			return const_reverse_iterator(0, 0);
		}
	}
	iterator insert(iterator it, const_iterator b, const_iterator e)
	{
		if (b < e) {
			size_t i;
			if (it == end()) {
				i = d.count;
			} else {
				i = it.ptr - d.array;
			}
			size_t next = 0;
			size_t n = e - b;
			if (n > d.capacity - d.count) {
				uninitialized_vector newvec;
				newvec.d.capacity = ((d.count + n + 7) & ~7) * 2;
				newvec.d.array = (T *)malloc(sizeof(T) * newvec.d.capacity);
				if (d.array && i > 0) {
					newvec.insert(newvec.end(), begin(), begin() + i);
				}
				newvec.insert(newvec.end(), b, e);
				next = newvec.size();
				if (d.array && d.count - i > 0) {
					newvec.insert(newvec.end(), begin() + i, end());
				}
				delete[] d.array;
				d = newvec.d;
				newvec.d = {};
			} else {
				memmove((T *)d.array + (i + n), (T *)d.array + i, sizeof(T) * (d.count - i));
				memcpy((T *)d.array + i, b.ptr, sizeof(T) * n);
				d.count += n;
				next = i + n;
			}
			return iterator(d.array + next, d.array + d.count);
		}
		return iterator(0, 0);
	}
	iterator insert(iterator it, T const v)
	{
		T const *p = &v;
		return insert(it, p, p + 1);
	}
	void push_back(T const &t)
	{
		T const *p = &t;
		insert(end(), p, p + 1);
	}
	void pop_back()
	{
		if (d.count > 0) {
			d.count--;
		}
	}
	T &operator [] (size_t i)
	{
		return d.array[i];
	}
	T const &operator [] (size_t i) const
	{
		return d.array[i];
	}
	void swap(uninitialized_vector &r)
	{
		std::swap(d, r.d);
	}
};

template <typename T> static inline void std::swap(uninitialized_vector<T> &a, uninitialized_vector<T> &b)
{
	a.swap(b);
}

#endif
