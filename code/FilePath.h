#pragma once

static const uint32_t kMaxFilePathLen = 256 + 1;

template<typename T>
class FilePathBasic : public std::basic_string<T>
{
public:
	using base_type = std::basic_string<T>;

	FilePathBasic() = default;

	FilePathBasic(const T* p, uint32_t n) : base_type(p, n)
	{
	}

	FilePathBasic(const T* p) : base_type(p)
	{
	}

	FilePathBasic(const FilePathBasic& x) : base_type(x)
	{
	}

	template<typename Iter>
	FilePathBasic(const Iter& pBegin, const Iter& pEnd) : base_type(pBegin, pEnd)
	{
	}

	FilePathBasic& operator/=(FilePathBasic& path);
	FilePathBasic& operator/=(const std::basic_string_view<T>& path);

	FilePathBasic operator/(const std::basic_string_view<T>& rightPath);

	FilePathBasic GetParentPath() const;
	FilePathBasic GetFileName() const;
	FilePathBasic GetStem() const;
	FilePathBasic GetExtension() const;

	FilePathBasic<T>& SetExtension(const std::basic_string_view<T>& ext);

	template<typename T>
	static bool IsSepartor(T c)
	{
		return false;
	}

	template<>
	static bool IsSepartor(char c)
	{
		return c == '/' || c == '\\';
	}

	template <>
	static bool IsSepartor(wchar_t c)
	{
		return c == L'/' || c == L'\\';
	}

	template <typename T>
	static T GetSepartor()
	{
		return nullptr;
	}

	template <>
	static const char* GetSepartor()
	{
		return "/";
	}

	template <>
	static const wchar_t* GetSepartor()
	{
		return L"/";
	}

	template <typename T>
	static bool IsDot(T c)
	{
		return false;
	}

	template <>
	static bool IsDot(char c)
	{
		return c == '.';
	}

	template <>
	static bool IsDot(wchar_t c)
	{
		return c == L'.';
	}

	template <typename T>
	static T GetDot()
	{
		return nullptr;
	}

	template <>
	static const char* GetDot()
	{
		return ".";
	}

	template <>
	static const wchar_t* GetDot()
	{
		return L".";
	}

	template <typename T>
	static bool IsSepartorOrDot(T c)
	{
		return IsSepartor(c) || IsDot(c);
	}

private:
};


using FilePath = FilePathBasic<char>;
using FilePathW = FilePathBasic<wchar_t>;


template<typename T>
inline FilePathBasic<T>& FilePathBasic<T>::operator/=(FilePathBasic<T>& path)
{
	std::basic_string_view<T> view(path.data(), path.length());
	return operator/=(view);
}


template <typename T>
inline FilePathBasic<T>& FilePathBasic<T>::operator/=(const std::basic_string_view<T>& path)
{
	if (base_type::empty())
	{
		base_type::assign(path.begin(), path.end());
	}
	else if (!path.empty())
	{
		if (!IsSepartor(base_type::back()))
			base_type::append(GetSepartor<const T*>());
		if (IsSepartor(path.front()))
			base_type::append(path.begin() + 1, path.end());
		else
			base_type::append(path.begin(), path.end());
	}

	return *this;
}


template <typename T>
inline FilePathBasic<T> FilePathBasic<T>::operator/(const std::basic_string_view<T>& rightPath)
{
	FilePathBasic ret = *this;
	ret /= rightPath;
	return ret;
}


template <typename T>
inline FilePathBasic<T> FilePathBasic<T>::GetParentPath() const
{
	auto iter = std::find_if(base_type::rbegin(), base_type::rend(), IsSepartor<T>);
	if (iter == base_type::rend())
		return FilePathBasic();

	return {base_type::begin(), iter.base() - 1};
}


template <typename T>
inline FilePathBasic<T> FilePathBasic<T>::GetFileName() const
{
	auto iter = std::find_if(base_type::rbegin(), base_type::rend(), IsSepartor<T>);
	return {iter.base(), base_type::end()};
}


template <typename T>
inline FilePathBasic<T> FilePathBasic<T>::GetStem() const
{
	auto seratorIter = std::find_if(base_type::rbegin(), base_type::rend(), IsSepartor<T>);
	auto seratorOrDotIter = std::find_if(base_type::rbegin(), base_type::rend(), IsSepartorOrDot<T>);
	if (seratorIter == seratorOrDotIter)
		return {seratorIter.base(), base_type::end()};
	else
		return {seratorIter.base(), seratorOrDotIter.base() - 1};
}


template <typename T>
inline FilePathBasic<T> FilePathBasic<T>::GetExtension() const
{
	auto iter = std::find_if(base_type::rbegin(), base_type::rend(), IsSepartorOrDot<T>);
	if (iter != base_type::rend() && IsDot(*iter))
		return FilePathBasic<T>(iter.base() - 1, base_type::end());

	return {};
}


template <typename T>
FilePathBasic<T>& FilePathBasic<T>::SetExtension(const std::basic_string_view<T>& ext)
{
	auto iter = std::find_if(base_type::rbegin(), base_type::rend(), IsSepartorOrDot<T>);
	if (iter == base_type::rend())
	{
		if (!ext.empty())
		{
			if (!IsDot(ext.front()))
				base_type::append(GetDot<const T*>());
			base_type::append(ext.begin(), ext.end());
		}
		return *this;
	}

	if (IsDot(*iter))
	{
		if (!ext.empty())
		{
			base_type::resize((uint32_t)(iter.base() - base_type::begin()));
			if (IsDot(ext.front()))
				base_type::append(ext.begin() + 1, ext.end());
			else
				base_type::append(ext.begin(), ext.end());
		}
		else
		{
			++iter;
			base_type::resize((uint32_t)(iter.base() - base_type::begin()));
		}
	}
	else
	{
		if (!ext.empty())
		{
			if (!IsDot(ext.front()))
				base_type::append(GetDot<const T*>());
			base_type::append(ext.begin(), ext.end());
		}
	}

	return *this;
}


inline FilePathW ConvertPath(const FilePath& path)
{
	FilePathW ret;
	ret.resize(path.length());
	size_t dummy = 0;
	mbstowcs_s(&dummy, (wchar_t*)ret.data(), ret.capacity() + 1, path.data(), path.length());
	return ret;
}


inline FilePath ConvertPath(const FilePathW& path)
{
	FilePath ret;
	ret.resize(path.length());
	size_t dummy = 0;
	wcstombs_s(&dummy, (char*)ret.data(), ret.capacity() + 1, path.data(), path.length());
	return ret;
}