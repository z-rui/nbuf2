#ifndef NBUF_HPP_
#define NBUF_HPP_

#include "nbuf.h"

#include <cstdint>
#include <iostream>
#if __cpp_lib_string_view
# include <string_view>
#endif
#include <type_traits>

namespace nbuf {

using buffer = ::nbuf_buf;

template <typename T, size_t sz = sizeof (T)>
struct scalar;

template <typename T>
struct scalar<T, 1> {
	scalar() = delete;
	operator T() const { return v_; }
	T operator=(T v) { v_ = v; return v; }
private:
	T v_;
};

template <typename T>
struct scalar<T, 2> {
	scalar() = delete;
	operator T() const {
		value_type u = { nbuf_u16(&v_.u) };
		return u.v;
	}
	T operator=(T v) {
		value_type u;
		u.v = v;
		nbuf_set_u16(&v_, u.u);
		return v;
	}
private:
	union value_type { uint16_t u; T v; } v_;
};

template <typename T>
struct scalar<T, 4> {
	scalar() = delete;
	operator T() const {
		value_type u = { nbuf_u32(&v_.u) };
		return u.v;
	}
	T operator=(T v) {
		value_type u;
		u.v = v;
		nbuf_set_u32(&v_, u.u);
		return v;
	}
private:
	union value_type { uint32_t u; T v; } v_;
};

template <typename T>
struct scalar<T, 8> {
	scalar() = delete;
	operator T() const {
		value_type u = { nbuf_u64(&v_.u) };
		return u.v;
	}
	T operator=(T v) {
		value_type u;
		u.v = v;
		nbuf_set_u64(&v_, u.u);
		return v;
	}
private:
	union value_type { uint64_t u; T v; } v_;
};

struct basic_array;
struct string;
template <typename T> struct scalar_array;
template <typename T> struct pointer_array;

struct object : ::nbuf_obj {
	object() : ::nbuf_obj{NULL, 0, 0, 0} {}
	operator bool() const {
		return ssize || psize;
	}
	size_t get(const buffer *buf, size_t offset = 0) {
		this->buf = const_cast<buffer *>(buf);
		this->offset = offset;
		return ::nbuf_get_obj(this);
	}
	template <typename U>
	scalar<U> *scalar_field(size_t offset) const {
		return reinterpret_cast<scalar<U> *>(::nbuf_obj_s(this, offset, sizeof (U)));
	}
	size_t pointer_field(object *o, size_t index) const {
		return ::nbuf_obj_p(o, this, index);
	}
	size_t set_pointer_field(size_t index, const object &o) const {
		return ::nbuf_obj_set_p(this, index, &o);
	}
	inline string string_field(size_t index) const;
	template <typename U>
	inline string set_string_field(size_t index, const U &s) const;
	size_t alloc(buffer *buf, size_t ssize, size_t psize) {
		this->buf = buf;
		this->ssize = ssize;
		this->psize = psize;
		return ::nbuf_alloc_obj(this);
	}
	size_t alloc(buffer *buf, size_t ssize, size_t psize, size_t n) {
		this->buf = buf;
		this->ssize = ssize;
		this->psize = psize;
		return ::nbuf_alloc_arr(this, n);
	}
};

struct basic_array : object {
	struct iterator {
		iterator (const object &o) : o_(o) {}
		iterator (const iterator &other) = default;
		iterator &operator =(const iterator &other) = default;
		bool operator ==(const iterator &other) { return o_.buf->base + o_.offset == other.o_.buf->base + other.o_.offset; }
		bool operator !=(const iterator &other) { return !(*this == other); }
		void advance(size_t n) { ::nbuf_advance(&o_, n); }
	protected:
		object o_;
	};
	operator bool () const { return size_ != 0; }
	basic_array(const object &o, size_t size) : object(o), size_(size) {}
	size_t size() const { return size_; }
	iterator begin() const { return iterator(*this); }
	iterator end() const { iterator it = begin(); it.advance(size_); return it; }
protected:
	size_t size_;
};

template <typename T>
struct scalar_array : basic_array {
	using basic_array::basic_array;
	struct iterator : basic_array::iterator {
		iterator(const basic_array::iterator &it) : basic_array::iterator(it) {}
		iterator (const iterator &other) = default;
		iterator &operator =(const iterator &other) = default;
		iterator &operator ++() { return *this += 1; }
		iterator &operator --() { return *this -= 1; }
		iterator &operator +=(size_t n) { advance(n); return *this; }
		iterator &operator -=(size_t n) { advance(-n); return *this; }
		T &operator*() const {
			return *reinterpret_cast<T *>(::nbuf_obj_s(&o_, 0, sizeof (T)));
		}
	};
	iterator begin() const {
		return basic_array::begin();
	}
	iterator end() const {
		return basic_array::end();
	}
	T &operator[](size_t i) const {
		iterator it = begin();
		it.advance(i);
		return *it;
	}
	static scalar_array alloc(buffer *buf, size_t n) {
		object o;
		o.buf = buf;
		o.ssize = sizeof (T);
		o.psize = 0;
		n = ::nbuf_alloc_arr(&o, n);
		return scalar_array(o, n);
	}
};

template <typename T>
struct pointer_array : basic_array {
	using basic_array::basic_array;
	struct iterator : public basic_array::iterator {
		iterator(const basic_array::iterator &it) : basic_array::iterator(it) {}
		iterator (const iterator &other) = default;
		iterator &operator =(const iterator &other) = default;
		iterator &operator ++() { return *this += 1; }
		iterator &operator --() { return *this -= 1; }
		iterator &operator +=(size_t n) { advance(n); return *this; }
		iterator &operator -=(size_t n) { advance(-n); return *this; }
		T operator*() const {
			T o;
			o.buf = o_.buf;
			o.offset = o_.offset;
			o.ssize = o_.ssize;
			o.psize = o_.psize;
			return o;
		}
	};
	iterator begin() const {
		return basic_array::begin();
	}
	iterator end() const {
		return basic_array::end();
	}
	T operator[](size_t i) const {
		iterator it = begin();
		it.advance(i);
		return *it;
	}
};

struct string : basic_array {
	using basic_array::basic_array;
#if __cpp_lib_string_view
	operator std::string_view() const {
		return std::string_view(data(), size());
	}
#endif
	size_t size() const {
		return size_ ? size_ - 1 : 0;
	}
	const char *data() const {
		return static_cast<const char *>(nbuf_obj_base(this));
	}
	const char *c_str() const {
		return size_ ? data() : "";
	}
	char operator[](size_t i) const {
		return data()[i];
	}
	friend std::ostream & operator<<(std::ostream &os, const string &s) {
		return os.write(s.data(), s.size());
	}
	static string alloc(buffer *buf, const char *s, size_t len)
	{
		object o;
		o.buf = buf;
		if (!::nbuf_alloc_str(&o, s, len))
			len = 0;
		return string(o, len);
	}
	template <size_t N>
	static string alloc(buffer *buf, const char (&s)[N]) {
		return alloc(buf, s, N-1);
	}
	template <typename U>
	static string alloc(buffer *buf, const U &s) {
		return alloc(buf, s.size(), s.data());
	}
};

struct string_proxy : string {
	template <typename U>
	string operator =(const U &s) {
		string o = string::alloc(&o, buf, s);
		if (o && nbuf_obj_set_p(this, 0, &o))
			return o;
		return string(object(), 0);
	}
private:
	using string::string;
	string_proxy(const string_proxy &) = default;
	friend struct string_array;
};

struct string_array : basic_array {
	using basic_array::basic_array;
	struct iterator : public basic_array::iterator {
		iterator(const basic_array::iterator &it) : basic_array::iterator(it) {}
		iterator (const iterator &other) = default;
		iterator &operator =(const iterator &other) = default;
		iterator &operator ++() { return *this += 1; }
		iterator &operator --() { return *this -= 1; }
		iterator &operator +=(size_t n) { advance(n); return *this; }
		iterator &operator -=(size_t n) { advance(-n); return *this; }
		string_proxy operator*() const {
			object o;
			size_t len = ::nbuf_obj_p(&o, &o_, 0);
			return string_proxy(o, len);
		}
	};
	iterator begin() const {
		return basic_array::begin();
	}
	iterator end() const {
		return basic_array::end();
	}
	string_proxy operator[](size_t i) {
		iterator it = begin();
		it.advance(i);
		return *it;
	}
	static string_array alloc(buffer *buf, size_t n) {
		object o;
		o.buf = buf;
		o.ssize = 0;
		o.psize = 1;
		n = ::nbuf_alloc_arr(&o, n);
		return string_array(o, n);
	}
};

string object::string_field(size_t index) const {
	object o;
	size_t len = ::nbuf::object::pointer_field(&o, 1);
	return string(o, len);
}

template <typename U>
string object::set_string_field(size_t index, const U &s) const {
	if (index < psize) {
		string o = string::alloc(buf, s);
		if (o && nbuf_obj_set_p(this, index, &o))
			return o;
	}
	return string(object(), 0);
}

}  // namespace nbuf

#endif  // NBUF_HPP_
