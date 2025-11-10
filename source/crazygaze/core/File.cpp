#include "File.h"
#include "Logging.h"
#include "StringUtils.h"

namespace cz
{

File::~File()
{
	if (m_handle)
	{
		fclose(m_handle);
	}
}

std::unique_ptr<File> File::open(const fs::path& path, Mode mode)
{
	return openImpl(path, mode, true);
}

std::unique_ptr<File> File::try_open(const fs::path& path, Mode mode)
{
	return openImpl(path, mode, false);
}

std::unique_ptr<File> File::openImpl(const fs::path& path, Mode mode, bool raiseError)
{
	wchar_t fmodebuf[4];
	wchar_t* fmode = &fmodebuf[0];

	switch(mode)
	{
		case Mode::Read:
			*fmode = 'r';
			break;
		case Mode::Write:
			*fmode = 'w';
			break;
		case Mode::Append:
			*fmode = 'a';
			break;
		case Mode::ReadWriteExisting:
			// Opens for read write, but the file needs to exist
			*(fmode++) = 'r';
			*fmode = '+';
			break;
		case Mode::ReadWriteNew:
			*(fmode++) = 'w';
			*fmode = '+';
			break;
		case Mode::ReadWriteAppend:
			*(fmode++) = 'a';
			*fmode = '+';
			break;
	}

	*(++fmode) ='b';
	(++fmode)[0] = 0;

	FILE* handle = _wfopen(path.c_str(), fmodebuf);
	if (handle == NULL)
	{
		if (raiseError)
		{
			CZ_LOG(Main, Error, "Couldn't open file '{}', with mode '{}'.", path, static_cast<int>(mode));
		}
		return nullptr;
	}

	auto file = std::make_unique<File>(this_is_private{0});
	file->m_path = path;
	file->m_handle = handle;
	file->m_mode = mode;

	return file;
}

size_t File::write(const void* buffer, size_t size, size_t count)
{
	// Any mode other than Mode::Read is writable
	CZ_CHECK(m_mode != Mode::Read);

	size_t r = fwrite(buffer, size, count, m_handle);

	CZ_CHECK_F((size*count==0) || (r==count), "{} failed. Requested {} elements ({} bytes each), and did {}", __FUNCTION__, static_cast<int>(count), static_cast<int>(size), static_cast<int>(r));

	return r;
}

size_t File::read(void* buffer, size_t size, size_t count)
{
	// Any other modes other than Mode:Write & Mode::Append are readable
	CZ_CHECK(!(m_mode==Mode::Write || m_mode==Mode::Append));

	size_t r = fread(buffer, size, count, m_handle);
	CZ_CHECK_F((size*count==0) || r==count, "{} failed. Requested {} elements ({} bytes each), and did {}", __FUNCTION__, static_cast<int>(count), static_cast<int>(size), static_cast<int>(r));
	return r;
}

size_t File::write(const void* buffer, size_t bytes)
{
	return write(buffer, bytes, 1);
}

size_t File::read(void* buffer, size_t bytes)
{
	return read(buffer, bytes, 1) * bytes;
}

File::Buffer File::readAllImpl(const fs::path& path, bool raiseError)
{
	std::unique_ptr<File> in = openImpl(path, Mode::Read, raiseError);
	if (!in)
	{
		return {};
	}

	size_t dataSize = in->size();
	Buffer buffer(dataSize);
	if (!buffer.ptr)
	{
		return {};
	}

	if (in->read(buffer.ptr, dataSize) != dataSize)
	{
		if (raiseError)
		{
			CZ_LOG(Main, Error, "Failed to read file contents");
		}
		return {};
	}
	else
	{
		buffer.size = dataSize;
		return buffer;
	}
}

bool File::eof() const
{
	return (feof(m_handle)==0) ? false : true;
}

size_t File::tell() const
{
	long p = ftell(m_handle);
	CZ_CHECK(p != -1);
	return static_cast<size_t>(p);
}

bool File::seek(size_t offset, SeekMode seekMode)
{
	int smode = 0;

	switch(seekMode)
	{
		case SeekMode::Set:
			smode = SEEK_SET;
			break;
		case SeekMode::Current:
			smode = SEEK_CUR;
			break;
		case SeekMode::End:
			smode = SEEK_END;
	}

	int r = fseek(m_handle, static_cast<long>(offset), smode);
	if (r != 0)
	{
		CZ_LOG(Main, Error, "{} failed", __FUNCTION__);
	}

	return (r==0) ? true : false;
}

size_t File::size()
{
	size_t originalPos = tell();

	seek(0, SeekMode::End);
	size_t size = tell();
	seek(originalPos, SeekMode::Set);
	return size;
}

bool saveTextFile(const fs::path& path, std::string_view contents, bool saveOnlyIfChanged)
{
	// Changes the EOLs per OS
	std::string tmp = changeEOL(contents, CZ_WINDOWS ? EOL::Windows : EOL::Linux);

	// First, check if the contents are different. If they are the same, then we don't touch the file, so that it makes iterations faster for
	// when there is another Visual Studio instance open with the generated games.
	if (saveOnlyIfChanged)
	{
		File::Buffer current = File::readAll(path, false);
		if (current.to_string_view() == tmp)
		{
			return true;
		}
	}

	std::unique_ptr<File> out = File::open(path, File::Mode::ReadWriteNew);
	if (!out)
	{
		return false;
	}

	return out->write(tmp.data(), tmp.size()) == 1 ? true : false;
}

namespace
{

/*!
 * Given a file's path, it checks the last time the file was modified, and returns a string in the format:
 * YYYY.MM.DD-HH.mm.ss
 * E.g: 2025.01.25-00.00.00
 *
 * It returns an empty string if the file doesn't exist or there was an error.
 */
std::string getFileTimestamp(const std::filesystem::path& filename)
{
	std::error_code ec;
	if (!std::filesystem::exists(filename, ec))
	{
		return "";
	}

#if defined(_WIN32)
	WIN32_FILE_ATTRIBUTE_DATA fileInfo;
	if (!GetFileAttributesExW(filename.wstring().c_str(), GetFileExInfoStandard, &fileInfo))
	{
		return "";
	}

	// Convert FILETIME to ULARGE_INTEGER
	ULARGE_INTEGER ull;
	ull.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
	ull.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;

	// Convert to time_t (number of 100-nanosecond intervals since Jan 1, 1601)
	// 11644473600 is the number of seconds from 1601 to 1970
	time_t rawtime = (time_t)((ull.QuadPart / 10000000ULL) - 11644473600ULL);

#else
	struct stat st;
	if (stat(filename.string().c_str(), &st) != 0)
	{
		return "";
	}

	time_t rawtime = st.st_mtime;
#endif

	struct tm* gmt = gmtime(&rawtime);
	if (!gmt)
	{
		return "";
	}

	// Temporary buffer
	// Example of contents: 2025.01.25-23.01.01
	char buffer[32];

	snprintf(
		buffer, sizeof(buffer), "%04d.%02d.%02d-%02d.%02d.%02d", gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday, gmt->tm_hour,
		gmt->tm_min, gmt->tm_sec);

	return buffer;
}

}

bool renameFileToTimestamp(const std::filesystem::path& filename)
{
	std::error_code ec;
	if (!std::filesystem::exists(filename, ec))
	{
		return true;
	}

	std::string timestamp = getFileTimestamp(filename);
	std::filesystem::path dir = filename.parent_path();
	std::filesystem::path baseName = filename.stem();
	std::filesystem::path extension = filename.extension();

	std::filesystem::path newFilename = dir / std::format("{}-{}{}", baseName.string(), timestamp, extension.string());
	std::filesystem::rename(filename, newFilename, ec);
	if (ec)
	{
		CZ_LOG(Main, Error, "Error renaming '{}' to '{}'. Error={}", filename.string(), newFilename.string(), ec.message());
		return false;
	}

	return true;
}


} // namespace cz

